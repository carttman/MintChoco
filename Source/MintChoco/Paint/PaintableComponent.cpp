#include "Paint/PaintableComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Paint/PositionMapBaker.h"

namespace
{
	const FName BrushPaintIdParam(TEXT("BrushPaintId"));
	const FName BrushCenterParam(TEXT("BrushCenterLocal"));
	const FName BrushRadiusParam(TEXT("BrushRadiusLocal"));
	const FName BrushAxisUParam(TEXT("BrushAxisULocal"));
	const FName BrushAxisVParam(TEXT("BrushAxisVLocal"));
	const FName BrushStretchParam(TEXT("BrushStretch"));
	const FName BrushSeedParam(TEXT("BrushSeed"));
	const FName BrushImpactUParam(TEXT("BrushImpactU"));
	const FName BrushHeightAddParam(TEXT("BrushHeightAdd"));
	const FName PreviousPaintParam(TEXT("PreviousPaint"));
	const FName PaintRenderTargetParam(TEXT("PaintRT"));
	// The relief custom node reads the id buffer through its own TextureObjectParameter. It must
	// NOT share PaintRT's name: an override on a name used by both a sampler parameter and a
	// texture-object parameter only reaches the sampler one.
	const FName PaintIdMapParam(TEXT("PaintIdMap"));
	const FName PaintTexelSizeParam(TEXT("PaintTexelSize"));
	const FName PositionMapParam(TEXT("PositionMap"));
	const FName BoundsMinParam(TEXT("BoundsMin"));
	const FName BoundsSizeParam(TEXT("BoundsSize"));
}

UPaintableComponent::UPaintableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPaintableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!BrushMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: BrushMaterial is unset, painting disabled."), *GetReadableName());
		return;
	}

	TargetMesh = FindTargetMesh();
	if (!TargetMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no StaticMeshComponent on the owner to paint onto."), *GetReadableName());
		return;
	}

	PaintRenderTargets[0] = CreateIdBuffer(this, RenderTargetResolution);
	PaintRenderTargets[1] = CreateIdBuffer(this, RenderTargetResolution);
	FrontBufferIndex = 0;

	BrushMID = UMaterialInstanceDynamic::Create(BrushMaterial, this);

	// An unset SurfaceMaterial means "keep what the mesh already has and blend paint into it",
	// so the original look survives instead of being replaced by a stand-in.
	UMaterialInterface* const BaseMaterial = SurfaceMaterial
		? SurfaceMaterial.Get()
		: TargetMesh->GetMaterial(SurfaceMaterialSlot);
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: material slot %d is empty and no override was set."), *GetReadableName(), SurfaceMaterialSlot);
		return;
	}

	SurfaceMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	SurfaceMID->SetTextureParameterValue(PaintRenderTargetParam, GetPaintRenderTarget());
	SurfaceMID->SetTextureParameterValue(PaintIdMapParam, GetPaintRenderTarget());
	// The relief blur in MF_PaintOverlay measures its taps in texels, so it needs the actual size.
	SurfaceMID->SetScalarParameterValue(PaintTexelSizeParam, 1.0f / RenderTargetResolution);
	TargetMesh->SetMaterial(SurfaceMaterialSlot, SurfaceMID);

	PositionBaker = NewObject<UPositionMapBaker>(this);
	PositionBaker->OnBaked.BindUObject(this, &UPaintableComponent::OnPositionMapBaked);
	PositionBaker->Initialize(TargetMesh, UnwrapMaterial, RenderTargetResolution, UnwrapPlaneSize);
	PositionBaker->RequestBake();
}

void UPaintableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bPaintReady = false;
	PaintRenderTargets[0] = nullptr;
	PaintRenderTargets[1] = nullptr;
	BrushMID = nullptr;
	SurfaceMID = nullptr;
	TargetMesh = nullptr;

	if (PositionBaker)
	{
		PositionBaker->Shutdown();
		PositionBaker = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

UTextureRenderTarget2D* UPaintableComponent::CreateIdBuffer(UObject* Outer, int32 Resolution)
{
	// Built by hand instead of CreateRenderTarget2D because the ID buffer needs its sampler
	// settings fixed before the resource is created: bilinear filtering would invent team IDs
	// on every splat boundary, and sRGB would corrupt the ID -> byte round trip.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	// R stores the team id, G accumulates deposited paint height.
	Buffer->RenderTargetFormat = RTF_RG8;
	Buffer->ClearColor = PaintIdNoneColor;
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Resolution, Resolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

FPaintSplat UPaintableComponent::BuildSplatFromHit(
	const FHitResult& Hit,
	FVector IncidentVelocity,
	uint8 PaintId,
	float Volume) const
{
	FPaintSplat Splat;
	Splat.Location = Hit.ImpactPoint;
	Splat.Normal = Hit.ImpactNormal;
	Splat.IncidentVelocity = IncidentVelocity;
	Splat.PaintId = PaintId;
	Splat.Volume = Volume;
	Splat.Seed = FMath::Rand();
	return Splat;
}

void UPaintableComponent::ApplySplat(const FPaintSplat& Splat)
{
	if (!bPaintReady) return;

	const auto Shape = ComputeSplatShape(Splat);

	// The stamp needs a full 2D frame on the surface, not just a stretch axis. A near-round
	// stamp gains nothing from tangent alignment - it would just repeat one orientation every
	// click - so its rotation comes from the replicated seed instead: every client derives the
	// same frame and stamps the same shape.
	const FVector Normal = Splat.Normal.GetSafeNormal();
	FVector AxisU = Shape.TangentDirection;
	if (Shape.Stretch < MinAlignedStretch || AxisU.IsNearlyZero())
	{
		const FRandomStream Stream(Splat.Seed);
		const FVector Reference = FMath::Abs(Normal.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector Base = FVector::CrossProduct(Normal, Reference).GetSafeNormal();
		AxisU = Base.RotateAngleAxis(Stream.FRand() * 360.0f, Normal);
	}
	const FVector AxisV = FVector::CrossProduct(Normal, AxisU);

	const auto& MeshTransform = TargetMesh->GetComponentTransform();
	const auto ShiftedCenter = Splat.Location + Shape.TangentDirection * Shape.CenterShift;
	const auto LocalCenter = MeshTransform.InverseTransformPosition(ShiftedCenter);
	// A direction only needs the rotation undone; scale would just be normalized away again.
	const auto LocalAxisU = MeshTransform.InverseTransformVectorNoScale(AxisU);
	const auto LocalAxisV = MeshTransform.InverseTransformVectorNoScale(AxisV);
	// Uniform scale assumed
	const float LocalRadius = Shape.Radius / MeshTransform.GetScale3D().X;

	// The brush writes the id as a normalized byte; the R8 target stores it back as exactly PaintId.
	BrushMID->SetScalarParameterValue(
		BrushPaintIdParam,
		Splat.PaintId / 255.0f);
	BrushMID->SetVectorParameterValue(
		BrushCenterParam,
		FLinearColor(LocalCenter));
	BrushMID->SetScalarParameterValue(
		BrushRadiusParam,
		LocalRadius);
	BrushMID->SetVectorParameterValue(
		BrushAxisUParam,
		FLinearColor(LocalAxisU));
	BrushMID->SetVectorParameterValue(
		BrushAxisVParam,
		FLinearColor(LocalAxisV));
	BrushMID->SetScalarParameterValue(
		BrushStretchParam,
		Shape.Stretch);
	// Wrapped so sin()-based hashing in the shader keeps its precision for typed-in seeds.
	BrushMID->SetScalarParameterValue(
		BrushSeedParam,
		static_cast<float>(Splat.Seed % 65536));
	// The center slid ahead of the contact, so in stamp space the impact sits behind the
	// origin: its u coordinate, normalized by the stamp's long axis, anchors the spike field.
	BrushMID->SetScalarParameterValue(
		BrushImpactUParam,
		-Shape.CenterShift / FMath::Max(Shape.Radius * Shape.Stretch, UE_KINDA_SMALL_NUMBER));
	BrushMID->SetScalarParameterValue(BrushHeightAddParam, Splat.HeightAdd);
	BrushMID->SetTextureParameterValue(PreviousPaintParam, GetPaintRenderTarget());

	UTextureRenderTarget2D* const Back = PaintRenderTargets[1 - FrontBufferIndex];

	// One draw per splat rebinds the render target every call. Batching several splats between
	// Begin/EndDrawCanvasToRenderTarget is the fix, once a single frame produces more than one.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Back, BrushMID);

	FrontBufferIndex = 1 - FrontBufferIndex;

	if (SurfaceMID)
	{
		SurfaceMID->SetTextureParameterValue(PaintRenderTargetParam, Back);
		SurfaceMID->SetTextureParameterValue(PaintIdMapParam, Back);
	}
}

void UPaintableComponent::ApplySplatInRadius(
	const UObject* WorldContextObject, const FPaintSplat& Splat, float WorldRadius)
{
	UWorld* const World =
		GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, Splat.Location, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(WorldRadius));

	// Overlap results repeat an actor once per overlapping component, so dedupe on the
	// paintable itself before drawing.
	TSet<UPaintableComponent*> Painted;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* const Actor = Overlap.GetActor();
		UPaintableComponent* const Paintable =
			Actor ? Actor->FindComponentByClass<UPaintableComponent>() : nullptr;
		if (!Paintable)
		{
			continue;
		}

		bool bAlreadyPainted = false;
		Painted.Add(Paintable, &bAlreadyPainted);
		if (!bAlreadyPainted)
		{
			Paintable->ApplySplat(Splat);
		}
	}
}

void UPaintableComponent::ClearPaint()
{
	for (UTextureRenderTarget2D* const Buffer : PaintRenderTargets)
	{
		if (Buffer)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, Buffer, PaintIdNoneColor);
		}
	}
}

UTextureRenderTarget2D* UPaintableComponent::GetPositionRenderTarget() const
{
	return PositionBaker ? PositionBaker->GetPositionMap() : nullptr;
}

void UPaintableComponent::OnPositionMapBaked(UTextureRenderTarget2D* PositionMap)
{
	// The baker only exists once BeginPlay fully succeeded, and EndPlay tears it down before
	// releasing the MIDs, so a null here is a programmer error rather than a designer one.
	check(BrushMID && SurfaceMID && TargetMesh);

	// Identity transform in, local bounds out - the same box the material's ObjectLocalBounds
	// node reads, which is what makes the un-normalize in the brush line up.
	const auto LocalBounds = TargetMesh->CalcBounds(FTransform::Identity).GetBox();

	BrushMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	BrushMID->SetVectorParameterValue(
		BoundsMinParam,
		FLinearColor(LocalBounds.Min));
	BrushMID->SetVectorParameterValue(
		BoundsSizeParam,
		FLinearColor(LocalBounds.GetSize()));

	// Only the brush needs the map, but the surface getting it too is what lets the
	// M_DebugPosition override work with zero extra plumbing.
	SurfaceMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	// The paint normal differentiates the position map, so it needs the un-normalize scale too.
	SurfaceMID->SetVectorParameterValue(
		BoundsSizeParam,
		FLinearColor(LocalBounds.GetSize()));

	bPaintReady = true;
}

FPaintSplatShape UPaintableComponent::ComputeSplatShape(const FPaintSplat& Splat) const
{
	FPaintSplatShape Shape;

	const float Speed = Splat.IncidentVelocity.Size();
	const FVector Normal = Splat.Normal.GetSafeNormal();
	const FVector Incident = Speed > UE_KINDA_SMALL_NUMBER
		? Splat.IncidentVelocity / Speed
		: -Normal;

	const float CosTheta = FMath::Abs(FVector::DotProduct(Incident, Normal));

	Shape.Radius = FMath::Min(
		BaseRadius * FMath::Sqrt(FMath::Max(Splat.Volume, 0.0f)) + RadiusPerSpeed * Speed,
		MaxRadius);
	Shape.Stretch = FMath::Clamp(1.0f / FMath::Max(CosTheta, UE_KINDA_SMALL_NUMBER), 1.0f, MaxStretch);
	Shape.TangentDirection = (Incident - FVector::DotProduct(Incident, Normal) * Normal).GetSafeNormal();
	Shape.CenterShift = Shape.Radius * (Shape.Stretch - 1.0f) * CenterShiftScale;

	return Shape;
}

UStaticMeshComponent* UPaintableComponent::FindTargetMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UStaticMeshComponent>() : nullptr;
}
