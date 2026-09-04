#include "Paint/PaintableComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Paint/PaintDebugDraw.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintMapBaker.h"
#include "Paint/PaintSubsystem.h"

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
	const FName BrushDistRangeParam(TEXT("BrushDistRange"));
	const FName PreviousPaintParam(TEXT("PreviousPaint"));
	// The surface reads the paint buffer through a TextureObjectParameter. Keep that name unique:
	// an override on a name shared by a sampler parameter and a texture-object parameter only
	// reaches the sampler one.
	const FName PaintIdMapParam(TEXT("PaintIdMap"));
	const FName PaintTexelSizeParam(TEXT("PaintTexelSize"));
	const FName PaintDistRangeParam(TEXT("PaintDistRange"));
	const FName PositionMapParam(TEXT("PositionMap"));
	const FName BoundsMinParam(TEXT("BoundsMin"));
	const FName BoundsSizeParam(TEXT("BoundsSize"));
	const FName PaintEdgeFadeParam(TEXT("PaintEdgeFade"));
}

UPaintableComponent::UPaintableComponent()
{
	// Ticking only serves the debug overlays, so it stays off until one of them is on.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPaintableComponent::BeginPlay()
{
	Super::BeginPlay();

	TargetMesh = FindTargetMesh();
	if (!TargetMesh)
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no StaticMeshComponent on the owner to paint onto."), *GetReadableName());
		return;
	}

	// Identity transform in, local bounds out - the same box the material's ObjectLocalBounds
	// node reads, which is what makes the un-normalize in the brush line up.
	MeshLocalBounds = TargetMesh->CalcBounds(FTransform::Identity).GetBox();

	const FVector Scale3D = TargetMesh->GetComponentTransform().GetScale3D();
	if (!FMath::IsNearlyEqual(Scale3D.X, Scale3D.Y) || !FMath::IsNearlyEqual(Scale3D.X, Scale3D.Z))
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: non-uniform scale %s; splat sizes and coverage areas assume the X scale."),
			*GetReadableName(), *Scale3D.ToString());
	}

	// The grid needs only the mesh, so it is ready long before the maps; splats still wait for
	// bPaintReady, so nothing gets scored that was not drawn.
	if (CellGrid.BuildFromMesh(*TargetMesh, SurfaceMaterialSlot, CellSize, GetUniformScale(), MeshLocalBounds))
	{
		const FIntVector& Dims = CellGrid.GetDims();
		UE_LOG(LogPaint, Log, TEXT("%s: cell grid %d x %d x %d, %d surface cells, %.0f cm^2."),
			*GetReadableName(), Dims.X, Dims.Y, Dims.Z, CellGrid.GetSurfaceCellCount(), CellGrid.GetCoverage().TotalArea);
	}
	if (const auto Paint = GetWorld()->GetSubsystem<UPaintSubsystem>())
	{
		Paint->RegisterPaintable(this);
	}
	SetComponentTickEnabled(bDrawDebugCoverage || bDrawDebugCells);

	PaintRenderTargets[0] = CreateIdBuffer(this, RenderTargetResolution);
	PaintRenderTargets[1] = CreateIdBuffer(this, RenderTargetResolution);
	FrontBufferIndex = 0;

	// An unset SurfaceMaterial means "keep what the mesh already has and blend paint into it",
	// so the original look survives instead of being replaced by a stand-in.
	UMaterialInterface* const BaseMaterial = SurfaceMaterial
		? SurfaceMaterial.Get()
		: TargetMesh->GetMaterial(SurfaceMaterialSlot);
	if (!BaseMaterial)
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: material slot %d is empty and no override was set."), *GetReadableName(), SurfaceMaterialSlot);
		return;
	}

	SurfaceMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	SurfaceMID->SetTextureParameterValue(PaintIdMapParam, GetPaintRenderTarget());
	// The paint reads filter the buffer by hand in texel units, so they need the actual size.
	SurfaceMID->SetScalarParameterValue(PaintTexelSizeParam, 1.0f / RenderTargetResolution);
	// The reads decode the brush's distance encoding, so both sides must agree on its range.
	SurfaceMID->SetScalarParameterValue(PaintDistRangeParam, PaintDistanceRange);
	TargetMesh->SetMaterial(SurfaceMaterialSlot, SurfaceMID);

	MapBaker = NewObject<UPaintMapBaker>(this);
	MapBaker->OnBaked.BindUObject(this, &UPaintableComponent::OnMapsBaked);
	MapBaker->Initialize(TargetMesh, RenderTargetResolution, UnwrapPlaneSize, EdgeFadeTexels, EdgeFadeSeamFraction);
	MapBaker->RequestBake();
}

void UPaintableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const auto World = GetWorld())
	{
		if (const auto Paint = World->GetSubsystem<UPaintSubsystem>())
		{
			Paint->UnregisterPaintable(this);
		}
	}

	bPaintReady = false;
	PaintRenderTargets[0] = nullptr;
	PaintRenderTargets[1] = nullptr;
	PositionMap = nullptr;
	BrushMIDs.Empty();
	SurfaceMID = nullptr;
	TargetMesh = nullptr;

	if (MapBaker)
	{
		MapBaker->Shutdown();
		MapBaker = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UPaintableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetMesh || !CellGrid.IsBuilt())
	{
		return;
	}

	const FTransform& MeshTransform = TargetMesh->GetComponentTransform();
	if (bDrawDebugCoverage)
	{
		const FString Label = GetOwner() ? GetOwner()->GetActorNameOrLabel() : GetName();
		PaintDebug::DrawCoverageText(GetWorld(), MeshTransform, GetUniformScale(), MeshLocalBounds, CellGrid, Label);
	}
	if (bDrawDebugCells)
	{
		PaintDebug::DrawCells(GetWorld(), MeshTransform, GetUniformScale(), CellGrid);
	}
}

UTextureRenderTarget2D* UPaintableComponent::CreateIdBuffer(UObject* Outer, int32 Resolution)
{
	// Built by hand instead of CreateRenderTarget2D because the ID buffer needs its sampler
	// settings fixed before the resource is created: bilinear filtering would invent team IDs
	// on every splat boundary, and sRGB would corrupt the ID -> byte round trip.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	// R stores the paint id, G accumulates deposited paint height, B the distance to the
	// nearest paint edge in texels, encoded as 1 - d / PaintDistanceRange so that a cleared
	// or default texel (B = 0) reads as "far".
	Buffer->RenderTargetFormat = RTF_RGBA8;
	Buffer->ClearColor = PaintIdNoneColor;
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Resolution, Resolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

void UPaintableComponent::ApplySplat(const FPaintSplat& Splat)
{
	if (!bPaintReady)
	{
		return;
	}

	UMaterialInstanceDynamic* const BrushMID = GetBrushMID(Splat.BrushMaterial);
	if (!BrushMID)
	{
		return;
	}

	const FPaintLocalStamp Stamp = ComputeLocalStamp(Splat);

	// The brush writes the id as a normalized byte; the target stores it back as exactly PaintId.
	BrushMID->SetScalarParameterValue(BrushPaintIdParam, Splat.PaintId / 255.0f);
	BrushMID->SetVectorParameterValue(BrushCenterParam, FLinearColor(Stamp.Center));
	BrushMID->SetScalarParameterValue(BrushRadiusParam, Stamp.Radius);
	BrushMID->SetVectorParameterValue(BrushAxisUParam, FLinearColor(Stamp.AxisU));
	BrushMID->SetVectorParameterValue(BrushAxisVParam, FLinearColor(Stamp.AxisV));
	BrushMID->SetScalarParameterValue(BrushStretchParam, Stamp.Stretch);
	BrushMID->SetScalarParameterValue(BrushSeedParam, static_cast<float>(Splat.Seed));
	BrushMID->SetScalarParameterValue(BrushImpactUParam, Splat.ImpactU);
	BrushMID->SetScalarParameterValue(BrushHeightAddParam, Splat.HeightAdd);
	BrushMID->SetTextureParameterValue(PreviousPaintParam, GetPaintRenderTarget());

	UTextureRenderTarget2D* const Back = PaintRenderTargets[1 - FrontBufferIndex];

	// One draw per splat rebinds the render target every call. Batching several splats between
	// Begin/EndDrawCanvasToRenderTarget is the fix, once a single frame produces more than one.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Back, BrushMID);

	FrontBufferIndex = 1 - FrontBufferIndex;
	SurfaceMID->SetTextureParameterValue(PaintIdMapParam, Back);

	// Same stamp the brush just drew, so ownership can only differ from the picture by the
	// stamp's satellites and the cell resolution.
	CellGrid.Mark(Stamp, Splat.PaintId, CellStampFraction);
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
	CellGrid.ClearPaint();
}

void UPaintableComponent::SetDebugDraw(bool bText, bool bCells)
{
	bDrawDebugCoverage = bText;
	bDrawDebugCells = bCells;
	SetComponentTickEnabled(bText || bCells);
}

UStaticMeshComponent* UPaintableComponent::FindTargetMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UStaticMeshComponent>() : nullptr;
}

float UPaintableComponent::GetUniformScale() const
{
	// Uniform scale assumed everywhere a world length becomes a local one; BeginPlay warns once.
	return TargetMesh->GetComponentTransform().GetScale3D().X;
}

void UPaintableComponent::OnMapsBaked(UTextureRenderTarget2D* InPositionMap, UTextureRenderTarget2D* EdgeFadeMap)
{
	// The baker only exists once BeginPlay fully succeeded, and EndPlay tears it down before
	// releasing the surface instance, so a null here is a programmer error rather than a designer one.
	check(SurfaceMID && TargetMesh);

	PositionMap = InPositionMap;
	for (const auto& Entry : BrushMIDs)
	{
		PrimeBrushMID(*Entry.Value);
	}

	// Only the brush needs the map, but the surface getting it too is what lets the
	// M_DebugPosition override work with zero extra plumbing.
	SurfaceMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	// The paint normal differentiates the position map, so it needs the un-normalize scale too.
	SurfaceMID->SetVectorParameterValue(BoundsSizeParam, FLinearColor(MeshLocalBounds.GetSize()));
	if (EdgeFadeMap)
	{
		SurfaceMID->SetTextureParameterValue(PaintEdgeFadeParam, EdgeFadeMap);
	}

	bPaintReady = true;
}

UMaterialInstanceDynamic* UPaintableComponent::GetBrushMID(UMaterialInterface* BrushMaterial)
{
	if (!BrushMaterial)
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: a splat arrived without a brush material; check the source's brush profile."), *GetReadableName());
		return nullptr;
	}
	if (const TObjectPtr<UMaterialInstanceDynamic>* const Cached = BrushMIDs.Find(BrushMaterial))
	{
		return *Cached;
	}

	UMaterialInstanceDynamic* const BrushMID = UMaterialInstanceDynamic::Create(BrushMaterial, this);
	PrimeBrushMID(*BrushMID);
	BrushMIDs.Add(BrushMaterial, BrushMID);
	return BrushMID;
}

void UPaintableComponent::PrimeBrushMID(UMaterialInstanceDynamic& BrushMID) const
{
	// Everything about this surface the brush has to know; the per-splat values come with the splat.
	BrushMID.SetScalarParameterValue(BrushDistRangeParam, PaintDistanceRange);
	if (PositionMap)
	{
		BrushMID.SetTextureParameterValue(PositionMapParam, PositionMap);
		BrushMID.SetVectorParameterValue(BoundsMinParam, FLinearColor(MeshLocalBounds.Min));
		BrushMID.SetVectorParameterValue(BoundsSizeParam, FLinearColor(MeshLocalBounds.GetSize()));
	}
}

FPaintLocalStamp UPaintableComponent::ComputeLocalStamp(const FPaintSplat& Splat) const
{
	const FTransform& MeshTransform = TargetMesh->GetComponentTransform();
	const FVector AxisV = FVector::CrossProduct(FVector(Splat.Normal), FVector(Splat.AxisU));

	FPaintLocalStamp Stamp;
	Stamp.Center = MeshTransform.InverseTransformPosition(Splat.Location);
	// A direction only needs the rotation undone; scale would just be normalized away again.
	Stamp.AxisU = MeshTransform.InverseTransformVectorNoScale(Splat.AxisU);
	Stamp.AxisV = MeshTransform.InverseTransformVectorNoScale(AxisV);
	Stamp.Normal = MeshTransform.InverseTransformVectorNoScale(Splat.Normal);
	Stamp.Radius = Splat.Radius / GetUniformScale();
	Stamp.Stretch = Splat.Stretch;
	return Stamp;
}
