#include "Paint/PaintableComponent.h"

#include "Components/MeshComponent.h"
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
	const FName PreviousPaintParam(TEXT("PreviousPaint"));
	const FName PaintRenderTargetParam(TEXT("PaintRT"));
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
		UE_LOG(LogTemp, Warning, TEXT("%s: no MeshComponent on the owner to paint onto."), *GetReadableName());
		return;
	}

	PaintRenderTargets[0] = CreateIdBuffer();
	PaintRenderTargets[1] = CreateIdBuffer();
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
	TargetMesh->SetMaterial(SurfaceMaterialSlot, SurfaceMID);

	PositionBaker = NewObject<UPositionMapBaker>(this);
	PositionBaker->OnBaked.BindUObject(this, &UPaintableComponent::OnPositionMapBaked);
	PositionBaker->Initialize(TargetMesh, UnwrapMaterial, RenderTargetResolution, UnwrapPlaneSize);
	PositionBaker->RequestBake();

}

void UPaintableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

UTextureRenderTarget2D* UPaintableComponent::CreateIdBuffer()
{
	// Built by hand instead of CreateRenderTarget2D because the ID buffer needs its sampler
	// settings fixed before the resource is created: bilinear filtering would invent team IDs
	// on every splat boundary, and sRGB would corrupt the ID -> byte round trip.
	UTextureRenderTarget2D* const Buffer = NewObject<UTextureRenderTarget2D>(this);
	Buffer->RenderTargetFormat = RTF_R8;
	Buffer->ClearColor = FLinearColor(PaintIdNone / 255.0f, 0.0f, 0.0f);
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(RenderTargetResolution, RenderTargetResolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

void UPaintableComponent::BuildSplatFromHit(
	const FHitResult& Hit,
	FVector IncidentVelocity,
	uint8 PaintId,
	float Volume,
	FPaintSplat& OutSplat) const
{
	OutSplat.Location = Hit.ImpactPoint;
	OutSplat.Normal = Hit.ImpactNormal;
	OutSplat.IncidentVelocity = IncidentVelocity;
	OutSplat.PaintId = PaintId;
	OutSplat.Volume = Volume;
	OutSplat.Seed = FMath::Rand();
}

void UPaintableComponent::ApplySplat(const FPaintSplat& Splat)
{
	if (!PaintRenderTargets[0] || !PaintRenderTargets[1] || !BrushMID || !GetPositionRenderTarget()) return;

	const auto Shape = ComputeSplatShape(Splat);

	const auto& MeshTransform = TargetMesh->GetComponentTransform();
	const auto LocalCenter = MeshTransform.InverseTransformPosition(Splat.Location);
	// Uniform scale assumed
	const float LocalRadius = Shape.Radius / MeshTransform.GetScale3D().X;

	// The brush writes the id as a normalized byte; the R8 target stores it back as exactly PaintId.
	BrushMID->SetScalarParameterValue(
		BrushPaintIdParam,
		Splat.PaintId / 255.0f);
	BrushMID->SetVectorParameterValue(
		BrushCenterParam,
		FLinearColor(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 0.0f));
	BrushMID->SetScalarParameterValue(
		BrushRadiusParam,
		LocalRadius);
	BrushMID->SetTextureParameterValue(PreviousPaintParam, GetPaintRenderTarget());

	UTextureRenderTarget2D* const Back = PaintRenderTargets[1 - FrontBufferIndex];

	// One draw per splat rebinds the render target every call. Batching several splats between
	// Begin/EndDrawCanvasToRenderTarget is the fix, once a single frame produces more than one.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Back, BrushMID);

	FrontBufferIndex = 1 - FrontBufferIndex;

	if (SurfaceMID)
	{
		SurfaceMID->SetTextureParameterValue(PaintRenderTargetParam, Back);
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
			UKismetRenderingLibrary::ClearRenderTarget2D(
				this, Buffer, FLinearColor(PaintIdNone / 255.0f, 0.0f, 0.0f));
		}
	}
}

UTextureRenderTarget2D* UPaintableComponent::GetPositionRenderTarget() const
{
	return PositionBaker ? PositionBaker->GetPositionMap() : nullptr;
}

void UPaintableComponent::OnPositionMapBaked(UTextureRenderTarget2D* PositionMap)
{
	// Identity transform in, local bounds out - the same box the material's ObjectLocalBounds
	// node reads, which is what makes the un-normalize in the brush line up.
	const auto LocalBounds = TargetMesh->CalcBounds(FTransform::Identity).GetBox();
	const auto BoundsSize = LocalBounds.GetSize();
	if (BrushMID)
	{
		BrushMID->SetTextureParameterValue(PositionMapParam, PositionMap);
		BrushMID->SetVectorParameterValue(
			BoundsMinParam,
			FLinearColor(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z));
		BrushMID->SetVectorParameterValue(
			BoundsSizeParam,
			FLinearColor(BoundsSize.X, BoundsSize.Y, BoundsSize.Z));
	}
	if (SurfaceMID)
	{
		// Only the brush needs the map, but the surface getting it too is what lets the
		// M_DebugPosition override work with zero extra plumbing.
		SurfaceMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	}
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

	return Shape;
}

UMeshComponent* UPaintableComponent::FindTargetMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UMeshComponent>() : nullptr;
}
