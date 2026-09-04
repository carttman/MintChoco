#include "Paint/PaintableComponent.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"

#include "Paint/PaintCoverageSubsystem.h"
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
	const FName FadeTexelsParam(TEXT("FadeTexels"));
	const FName SeamFractionParam(TEXT("SeamFraction"));
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

	// Identity transform in, local bounds out - the same box the material's ObjectLocalBounds
	// node reads, which is what makes the un-normalize in the brush line up.
	MeshLocalBounds = TargetMesh->CalcBounds(FTransform::Identity).GetBox();

	// The grid needs only the mesh, so it is ready long before the position map; splats still
	// wait for bPaintReady, so nothing gets scored that was not drawn.
	BuildCellGrid();
	if (const auto Coverage = GetWorld()->GetSubsystem<UPaintCoverageSubsystem>())
	{
		Coverage->RegisterPaintable(this);
	}
	SetComponentTickEnabled(bDrawDebugCoverage || bDrawDebugCells);

	PaintRenderTargets[0] = CreateIdBuffer(this, RenderTargetResolution);
	PaintRenderTargets[1] = CreateIdBuffer(this, RenderTargetResolution);
	FrontBufferIndex = 0;

	BrushMID = UMaterialInstanceDynamic::Create(BrushMaterial, this);
	BrushMID->SetScalarParameterValue(BrushDistRangeParam, PaintDistanceRange);

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
	SurfaceMID->SetTextureParameterValue(PaintIdMapParam, GetPaintRenderTarget());
	// The paint reads filter the buffer by hand in texel units, so they need the actual size.
	SurfaceMID->SetScalarParameterValue(PaintTexelSizeParam, 1.0f / RenderTargetResolution);
	// The reads decode the brush's distance encoding, so both sides must agree on its range.
	SurfaceMID->SetScalarParameterValue(PaintDistRangeParam, PaintDistanceRange);
	TargetMesh->SetMaterial(SurfaceMaterialSlot, SurfaceMID);

	PositionBaker = NewObject<UPositionMapBaker>(this);
	PositionBaker->OnBaked.BindUObject(this, &UPaintableComponent::OnPositionMapBaked);
	PositionBaker->Initialize(TargetMesh, UnwrapMaterial, RenderTargetResolution, UnwrapPlaneSize);
	PositionBaker->RequestBake();
}

void UPaintableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const auto World = GetWorld())
	{
		if (const auto Coverage = World->GetSubsystem<UPaintCoverageSubsystem>())
		{
			Coverage->UnregisterPaintable(this);
		}
	}

	bPaintReady = false;
	PaintRenderTargets[0] = nullptr;
	PaintRenderTargets[1] = nullptr;
	EdgeFadeRenderTarget = nullptr;
	EdgeFadeMID = nullptr;
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

UTextureRenderTarget2D* UPaintableComponent::CreateFadeBuffer(UObject* Outer, int32 Resolution)
{
	// A plain scalar, so unlike the id buffer it can be filtered by the sampler. White means
	// "no fade", which is also what the surface material assumes until the bake delivers.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	Buffer->RenderTargetFormat = RTF_R8;
	Buffer->ClearColor = FLinearColor::White;
	Buffer->Filter = TF_Bilinear;
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
	const FPaintLocalStamp Stamp = ComputeLocalStamp(Splat, Shape);

	// The brush writes the id as a normalized byte; the R8 target stores it back as exactly PaintId.
	BrushMID->SetScalarParameterValue(
		BrushPaintIdParam,
		Splat.PaintId / 255.0f);
	BrushMID->SetVectorParameterValue(
		BrushCenterParam,
		FLinearColor(Stamp.Center));
	BrushMID->SetScalarParameterValue(
		BrushRadiusParam,
		Stamp.Radius);
	BrushMID->SetVectorParameterValue(
		BrushAxisUParam,
		FLinearColor(Stamp.AxisU));
	BrushMID->SetVectorParameterValue(
		BrushAxisVParam,
		FLinearColor(Stamp.AxisV));
	BrushMID->SetScalarParameterValue(
		BrushStretchParam,
		Stamp.Stretch);
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
		SurfaceMID->SetTextureParameterValue(PaintIdMapParam, Back);
	}

	// Same stamp the brush just drew, so ownership can only differ from the picture by the
	// stamp's satellites and the cell resolution.
	CellGrid.Mark(Stamp, Splat.PaintId, CellStampFraction);
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
	CellGrid.ClearPaint();
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

	const auto& LocalBounds = MeshLocalBounds;

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

	BakeEdgeFade(PositionMap);

	bPaintReady = true;
}

void UPaintableComponent::BakeEdgeFade(UTextureRenderTarget2D* PositionMap)
{
	if (!EdgeFadeMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: EdgeFadeMaterial is unset, displacement will tear at island edges."), *GetReadableName());
		return;
	}

	if (!EdgeFadeRenderTarget)
	{
		EdgeFadeRenderTarget = CreateFadeBuffer(this, RenderTargetResolution);
	}
	if (!EdgeFadeMID)
	{
		EdgeFadeMID = UMaterialInstanceDynamic::Create(EdgeFadeMaterial, this);
	}

	EdgeFadeMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	EdgeFadeMID->SetScalarParameterValue(FadeTexelsParam, EdgeFadeTexels);
	EdgeFadeMID->SetScalarParameterValue(SeamFractionParam, EdgeFadeSeamFraction);

	// The map only depends on the unwrap, never on the paint, so one draw per bake is enough.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, EdgeFadeRenderTarget, EdgeFadeMID);
	SurfaceMID->SetTextureParameterValue(PaintEdgeFadeParam, EdgeFadeRenderTarget);
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

FPaintLocalStamp UPaintableComponent::ComputeLocalStamp(const FPaintSplat& Splat, const FPaintSplatShape& Shape) const
{
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

	FPaintLocalStamp Stamp;
	Stamp.Center = MeshTransform.InverseTransformPosition(ShiftedCenter);
	// A direction only needs the rotation undone; scale would just be normalized away again.
	Stamp.AxisU = MeshTransform.InverseTransformVectorNoScale(AxisU);
	Stamp.AxisV = MeshTransform.InverseTransformVectorNoScale(AxisV);
	Stamp.Normal = MeshTransform.InverseTransformVectorNoScale(Normal);
	// Uniform scale assumed
	Stamp.Radius = Shape.Radius / MeshTransform.GetScale3D().X;
	Stamp.Stretch = Shape.Stretch;
	return Stamp;
}

void UPaintableComponent::BuildCellGrid()
{
	const UStaticMesh* const Mesh = TargetMesh->GetStaticMesh();
	const FStaticMeshRenderData* const RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no render data on the mesh, coverage disabled."), *GetReadableName());
		return;
	}

	// LOD 0 is the Nanite fallback on a Nanite mesh, which is plenty for cells this coarse. In a
	// cooked build these buffers only exist on the CPU if the mesh asset has Allow CPU Access on.
	const auto& LOD = RenderData->LODResources[0];
	const auto& PositionBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const auto& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	const auto IndexView = LOD.IndexBuffer.GetArrayView();
	const uint32 VertexCount = PositionBuffer.GetNumVertices();
	if (VertexCount == 0 || IndexView.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: mesh geometry is not CPU-readable (enable Allow CPU Access on %s), coverage disabled."),
			*GetReadableName(), *GetNameSafe(Mesh));
		return;
	}

	// Only the slot that shows paint counts; a second material on the mesh is not paintable.
	TArray<uint32> Indices;
	for (const FStaticMeshSection& Section : LOD.Sections)
	{
		const int32 End = Section.FirstIndex + Section.NumTriangles * 3;
		if (Section.MaterialIndex != SurfaceMaterialSlot || End > IndexView.Num())
		{
			continue;
		}
		Indices.Reserve(Indices.Num() + Section.NumTriangles * 3);
		for (int32 Index = Section.FirstIndex; Index < End; ++Index)
		{
			Indices.Add(IndexView[Index]);
		}
	}
	if (Indices.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: material slot %d has no triangles, coverage disabled."),
			*GetReadableName(), SurfaceMaterialSlot);
		return;
	}

	TArray<FVector3f> Normals;
	if (VertexBuffer.GetNumVertices() == VertexCount)
	{
		Normals.SetNumUninitialized(VertexCount);
		for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
		{
			Normals[Vertex] = FVector3f(VertexBuffer.VertexTangentZ(Vertex));
		}
	}
	const TArrayView<const FVector3f> Positions(
		static_cast<const FVector3f*>(PositionBuffer.GetVertexData()), VertexCount);

	// Uniform scale assumed, as everywhere else in the component.
	const float Scale = TargetMesh->GetComponentTransform().GetScale3D().X;
	CellGrid.Build(MeshLocalBounds, CellSize / Scale, Scale * Scale, Positions, Normals, Indices);

	const FIntVector& Dims = CellGrid.GetDims();
	UE_LOG(LogTemp, Log, TEXT("%s: cell grid %d x %d x %d, %d surface cells, %.0f cm^2."),
		*GetReadableName(), Dims.X, Dims.Y, Dims.Z, CellGrid.GetSurfaceCellCount(), CellGrid.GetCoverage().TotalArea);
}

void UPaintableComponent::SetDebugDraw(bool bText, bool bCells)
{
	bDrawDebugCoverage = bText;
	bDrawDebugCells = bCells;
	SetComponentTickEnabled(bText || bCells);
}

void UPaintableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TargetMesh && CellGrid.IsBuilt())
	{
		DrawDebugCoverage();
	}
}

void UPaintableComponent::DrawDebugCoverage() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* const World = GetWorld();
	const FTransform& MeshTransform = TargetMesh->GetComponentTransform();
	const FVector LocalCenter = MeshLocalBounds.GetCenter();
	const FVector LocalExtent = MeshLocalBounds.GetExtent();
	const float Scale = MeshTransform.GetScale3D().X;

	// Duration 0 lives exactly one frame, so re-issuing every tick keeps the text current
	// without ever stacking stale copies.
	if (bDrawDebugCoverage)
	{
		const FString Label = GetOwner() ? GetOwner()->GetActorNameOrLabel() : GetName();
		DrawDebugString(
			World, MeshTransform.TransformPosition(LocalCenter),
			FString::Printf(TEXT("%s  %s"), *Label, *CellGrid.GetCoverage().ToString()),
			nullptr, FColor::White, 0.0f, true);

		constexpr float MarginWorld = 20.0f;
		for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
		{
			const auto Face = static_cast<EPaintFaceDirection>(Direction);
			const FPaintCoverage Coverage = CellGrid.GetCoverage(Face);
			if (Coverage.TotalArea <= 0.0f)
			{
				continue;
			}
			const FVector Axis = PaintFaceDirectionVector(Face);
			const double Reach = FVector::DotProduct(Axis.GetAbs(), LocalExtent) + MarginWorld / Scale;
			DrawDebugString(
				World, MeshTransform.TransformPosition(LocalCenter + Axis * Reach),
				FString::Printf(TEXT("%s  %s"), PaintFaceDirectionName(Face), *Coverage.ToString()),
				nullptr, PaintIdDebugColor(PaintIdNone), 0.0f, true);
		}
	}

	if (bDrawDebugCells)
	{
		// A cell is a patch of surface, so it is drawn as a slab centered on that surface, facing
		// its direction, rather than as the voxel it lives in (which straddles the surface). Thick
		// enough to stand clear of the displaced paint, which would otherwise swallow a thin one.
		const float HalfCell = CellGrid.GetCellSize() * Scale * 0.45f;
		const FVector Extent(HalfCell, HalfCell, HalfCell * 0.5f);
		CellGrid.ForEachSurfaceCell([&](const FVector& SurfaceCenter, EPaintFaceDirection Direction, uint8 PaintId, float)
		{
			const bool bPainted = PaintId != PaintIdNone;
			const FQuat FaceRotation = FRotationMatrix::MakeFromZ(PaintFaceDirectionVector(Direction)).ToQuat();
			DrawDebugBox(
				World, MeshTransform.TransformPosition(SurfaceCenter), Extent, MeshTransform.GetRotation() * FaceRotation,
				bPainted ? PaintIdDebugColor(PaintId) : FColor(70, 70, 70),
				false, 0.0f, 0, bPainted ? 1.0f : 0.0f);
		});
	}
#endif
}
