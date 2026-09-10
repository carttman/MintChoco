#include "Paint/PaintableComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#include "Paint/PaintDebugDraw.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintSettings.h"
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
	/** One rectangle per direction, enum order: uv offset in xy, uv scale in zw, all zero when the direction is off. */
	const FName PaintIslandParams[PaintFaceDirectionCount] = {
		FName(TEXT("PaintIsland_Front")), FName(TEXT("PaintIsland_Back")),
		FName(TEXT("PaintIsland_Right")), FName(TEXT("PaintIsland_Left")),
		FName(TEXT("PaintIsland_Up")), FName(TEXT("PaintIsland_Down")),
	};

	FString DirectionMaskToString(uint8 Mask)
	{
		FString Result;
		for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
		{
			const auto Face = static_cast<EPaintFaceDirection>(Direction);
			if (Mask & PaintDirectionBit(Face))
			{
				Result += FString::Printf(TEXT("%s%s"), Result.IsEmpty() ? TEXT("") : TEXT("+"), PaintDebug::FaceName(Face));
			}
		}
		return Result.IsEmpty() ? TEXT("none") : Result;
	}
}

UPaintableComponent::UPaintableComponent()
{
	// Ticking only serves the debug overlays, so it stays off until one of them is on. In the
	// editor the same tick keeps the cell overlay on a surface the designer is still moving.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = true;
}

bool UPaintableComponent::IsInEditorWorld() const
{
	const UWorld* const World = GetWorld();
	return World && World->WorldType == EWorldType::Editor;
}

void UPaintableComponent::OnRegister()
{
	Super::OnRegister();

	// A level designer wants to see the score cells while placing the actor, long before play.
	if (IsInEditorWorld() && bDrawDebugCells)
	{
		PrepareSurface();
		UpdateTickEnabled();
	}
}

#if WITH_EDITOR
void UPaintableComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (IsInEditorWorld())
	{
		if (bDrawDebugCells)
		{
			PrepareSurface();
		}
		UpdateTickEnabled();
	}
}
#endif

bool UPaintableComponent::PrepareSurface()
{
	TargetMesh = FindTargetMesh();
	if (!TargetMesh)
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no StaticMeshComponent on the owner to paint onto."), *GetReadableName());
		return false;
	}

	// Identity transform in, local bounds out - the same box the material's ObjectLocalBounds
	// node reads, which is what makes the un-normalize in the brush line up.
	MeshLocalBounds = TargetMesh->CalcBounds(FTransform::Identity).GetBox();

	PreparedTransform = TargetMesh->GetComponentTransform();
	const FVector RawScale = PreparedTransform.GetScale3D();
	Scale3D = RawScale.GetAbs();
	if (RawScale.GetMin() < 0.0)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: mirrored scale %s; paint treats it as %s, so stamps land mirrored on this surface."),
			*GetReadableName(), *RawScale.ToString(), *Scale3D.ToString());
	}

	EnabledDirections = ResolveEnabledDirections();
	UE_LOG(LogPaint, Log, TEXT("%s: keeps paint on %s."), *GetReadableName(), *DirectionMaskToString(EnabledDirections));

	// The grid needs only the mesh, so it is ready long before the atlas; splats still wait for
	// bPaintReady, so nothing gets scored that was not drawn.
	if (CellGrid.BuildFromMesh(*TargetMesh, SurfaceMaterialSlot, UPaintSettings::Get().ScoreCellSize, Scale3D, MeshLocalBounds, EnabledDirections))
	{
		const FIntVector& Dims = CellGrid.GetDims();
		UE_LOG(LogPaint, Log, TEXT("%s: cell grid %d x %d x %d, %d surface cells, %.0f cm^2."),
			*GetReadableName(), Dims.X, Dims.Y, Dims.Z, CellGrid.GetSurfaceCellCount(), CellGrid.GetCoverage().TotalArea);
	}
	return true;
}

void UPaintableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PrepareSurface()) return;

	const auto& Settings = UPaintSettings::Get();
	const auto Paint = GetWorld()->GetSubsystem<UPaintSubsystem>();
	if (Paint)
	{
		Paint->RegisterPaintable(this);
	}
	UpdateTickEnabled();

	// A dedicated server has no picture to keep, only the score; the grid alone is enough. So is
	// a surface that keeps no direction: every splat on it is an effect, never a buffer write.
	if (IsRunningDedicatedServer() || EnabledDirections == 0)
	{
		bPaintReady = true;
		return;
	}

	// The gutter has to hold the whole edge fade plus the brush's distance ramp, or a splat at an
	// island's edge would bleed into its neighbour.
	const int32 Pad = FMath::Max(
		Settings.IslandPaddingTexels,
		FMath::CeilToInt(Settings.EdgeFadeTexels) + FMath::CeilToInt(PaintDistanceRange) + 1);
	Layout = FPaintIslandLayout::Build(
		MeshLocalBounds, Scale3D, EnabledDirections, Settings.PaintTexelSizeCm, Pad,
		Settings.MinRenderTargetSize, Settings.MaxRenderTargetSize);
	if (Layout.IsEmpty())
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no atlas layout, paint disabled."), *GetReadableName());
		return;
	}
	UE_LOG(LogPaint, Log, TEXT("%s: atlas %s"), *GetReadableName(), *Layout.ToString());

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

	PaintRenderTarget = UPaintSubsystem::CreatePaintBuffer(this, Layout.AtlasSize);

	SurfaceMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	SurfaceMID->SetTextureParameterValue(PaintIdMapParam, PaintRenderTarget);
	// The paint reads filter the buffer by hand in texel units, so they need the actual size.
	SurfaceMID->SetScalarParameterValue(PaintTexelSizeParam, 1.0f / Layout.AtlasSize);
	// The reads decode the brush's distance encoding, so both sides must agree on its range.
	SurfaceMID->SetScalarParameterValue(PaintDistRangeParam, PaintDistanceRange);
	// The reader normalizes the pixel's local position with these and differentiates the position
	// atlas in unscaled local space, letting the Local -> World transform apply the scale.
	SurfaceMID->SetVectorParameterValue(BoundsMinParam, FLinearColor(MeshLocalBounds.Min));
	SurfaceMID->SetVectorParameterValue(BoundsSizeParam, FLinearColor(MeshLocalBounds.GetSize()));
	for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
	{
		const FPaintIsland* const Island = Layout.Find(static_cast<EPaintFaceDirection>(Direction));
		const FVector4f Param = Island ? Island->ToShaderParam(Layout.AtlasSize) : FVector4f::Zero();
		SurfaceMID->SetVectorParameterValue(PaintIslandParams[Direction], FLinearColor(Param.X, Param.Y, Param.Z, Param.W));
	}
	TargetMesh->SetMaterial(SurfaceMaterialSlot, SurfaceMID);

	if (Paint)
	{
		bAtlasRequested = true;
		Paint->RequestAtlas(
			*TargetMesh, SurfaceMaterialSlot, MeshLocalBounds, Layout,
			FPaintAtlasReady::CreateUObject(this, &UPaintableComponent::OnAtlasReady));
	}
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
	bAtlasRequested = false;
	PendingSplats.Empty();
	PaintRenderTarget = nullptr;
	PositionMap = nullptr;
	EdgeFadeMap = nullptr;
	BrushMIDs.Empty();
	SurfaceMID = nullptr;
	TargetMesh = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UPaintableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bPaintReady && !PendingSplats.IsEmpty())
	{
		const int32 Count = FMath::Min(MaxSplatsPerTick, PendingSplats.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			DrawSplat(PendingSplats[Index]);
		}
		PendingSplats.RemoveAt(0, Count, EAllowShrinking::No);
		UpdateTickEnabled();
	}

	if (!TargetMesh) return;

	// In the editor the surface follows the designer's hand: a moved or rescaled actor gets its
	// grid rebuilt so the cells stay honest.
	if (IsInEditorWorld() && !TargetMesh->GetComponentTransform().Equals(PreparedTransform))
	{
		PrepareSurface();
	}
	if (!CellGrid.IsBuilt())
	{
		return;
	}

	const FTransform ScaledLocalToWorld = GetScaledLocalToWorld();
	if (bDrawDebugCoverage && !IsInEditorWorld())
	{
		const FString Label = GetOwner() ? GetOwner()->GetActorNameOrLabel() : GetName();
		PaintDebug::DrawCoverageText(GetWorld(), ScaledLocalToWorld, GetScaledBounds(), CellGrid, Label);
	}
	if (bDrawDebugCells)
	{
		PaintDebug::DrawCells(GetWorld(), ScaledLocalToWorld, CellGrid);
	}
}

void UPaintableComponent::ApplySplat(const FPaintSplat& Splat)
{
	// A surface that never got past BeginPlay will never be ready, so nothing waits on it.
	if (!TargetMesh || (!bPaintReady && !bAtlasRequested))
	{
		return;
	}

	if (bPaintReady && PendingSplats.IsEmpty())
	{
		DrawSplat(Splat);
		return;
	}

	PendingSplats.Add(Splat);
	UpdateTickEnabled();
}

void UPaintableComponent::UpdateTickEnabled()
{
	const bool bOverlay = IsInEditorWorld() ? bDrawDebugCells : (bDrawDebugCoverage || bDrawDebugCells);
	SetComponentTickEnabled(bOverlay || !PendingSplats.IsEmpty());
}

void UPaintableComponent::DrawSplat(const FPaintSplat& Splat)
{
	const FPaintLocalStamp Stamp = ComputeLocalStamp(Splat);

	// Same stamp the brush draws, so ownership can only differ from the picture by the stamp's
	// satellites and the cell resolution. Marked first: the score exists even where there is no
	// picture (a dedicated server).
	CellGrid.Mark(Stamp, Splat.PaintId, CellStampFraction);

	if (!SurfaceMID || !PaintRenderTarget)
	{
		return;
	}

	FStampRects Rects;
	BuildStampRects(Stamp, Rects);
	if (Rects.IsEmpty())
	{
		return;
	}

	UMaterialInstanceDynamic* const BrushMID = GetBrushMID(Splat.BrushMaterial);
	if (!BrushMID)
	{
		return;
	}

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
	BrushMID->SetTextureParameterValue(PreviousPaintParam, PaintRenderTarget);

	DrawStampRects(*BrushMID, Rects);
}

void UPaintableComponent::BuildStampRects(const FPaintLocalStamp& Stamp, FStampRects& OutRects) const
{
	const FBox ScaledBounds = GetScaledBounds();
	const FVector Size = ScaledBounds.GetSize();
	// The brush keeps an exact edge distance this far outside the stamp, so the rectangle has to
	// reach that far too or the ramp would be cut off.
	const double Margin = (PaintDistanceRange + 1.0f) * Layout.TexelCm;

	// Bounding box of the stamp ellipsoid: Radius * Stretch along U, Radius along V and the normal.
	FVector Extent;
	FVector Low;
	FVector High;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Extent[Axis] = Stamp.Radius * (Stamp.Stretch * FMath::Abs(Stamp.AxisU[Axis]) + FMath::Abs(Stamp.AxisV[Axis]) + FMath::Abs(Stamp.Normal[Axis])) + Margin;
		const double Inv = Size[Axis] > UE_DOUBLE_SMALL_NUMBER ? 1.0 / Size[Axis] : 0.0;
		Low[Axis] = (Stamp.Center[Axis] - Extent[Axis] - ScaledBounds.Min[Axis]) * Inv;
		High[Axis] = (Stamp.Center[Axis] + Extent[Axis] - ScaledBounds.Min[Axis]) * Inv;
	}

	for (const FPaintIsland& Island : Layout.Islands)
	{
		const FVector2D From = Island.ProjectNormalized(Low);
		const FVector2D To = Island.ProjectNormalized(High);
		FIntRect Rect(
			FMath::FloorToInt(From.X), FMath::FloorToInt(From.Y),
			FMath::CeilToInt(To.X), FMath::CeilToInt(To.Y));
		Rect.Clip(Island.Rect);
		if (Rect.Area() > 0)
		{
			OutRects.Add(Rect);
		}
	}
}

void UPaintableComponent::DrawStampRects(UMaterialInstanceDynamic& BrushMID, const FStampRects& Rects)
{
	const auto Paint = GetWorld()->GetSubsystem<UPaintSubsystem>();
	const auto Scratch = Paint ? Paint->GetScratchTarget(Layout.AtlasSize) : nullptr;
	if (!Scratch) return;

	// The brush samples the previous paint and the position atlas by its own texture coordinate,
	// so each tile's coordinates are the atlas rectangle it covers: the pixel under an atlas texel
	// reads exactly that texel.
	const double AtlasSize = Layout.AtlasSize;
	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Scratch, Canvas, CanvasSize, Context);
	if (Canvas)
	{
		for (const FIntRect& Rect : Rects)
		{
			Canvas->K2_DrawMaterial(
				&BrushMID, FVector2D(Rect.Min), FVector2D(Rect.Size()),
				FVector2D(Rect.Min) / AtlasSize, FVector2D(Rect.Size()) / AtlasSize);
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	// The scratch draw read this surface's buffer; copying the rectangles back is what makes the
	// splat stick. Render commands run in order, so the next splat's read sees this copy.
	FTextureRenderTargetResource* const Source = Scratch->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* const Destination = PaintRenderTarget->GameThread_GetRenderTargetResource();
	if (!Source || !Destination)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(PaintCopySplatRects)(
		[Source, Destination, CopyRects = TArray<FIntRect>(Rects)](FRHICommandListImmediate& RHICmdList)
		{
			FRHITexture* const SourceTexture = Source->GetRenderTargetTexture();
			FRHITexture* const DestinationTexture = Destination->GetRenderTargetTexture();
			if (!SourceTexture || !DestinationTexture)
			{
				return;
			}
			for (const FIntRect& Rect : CopyRects)
			{
				FRHICopyTextureInfo Info;
				Info.Size = FIntVector(Rect.Width(), Rect.Height(), 1);
				Info.SourcePosition = FIntVector(Rect.Min.X, Rect.Min.Y, 0);
				Info.DestPosition = Info.SourcePosition;
				TransitionAndCopyTexture(RHICmdList, SourceTexture, DestinationTexture, Info);
			}
		});
}

void UPaintableComponent::ClearPaint()
{
	PendingSplats.Empty();
	UpdateTickEnabled();
	if (PaintRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, PaintRenderTarget, PaintIdNoneColor);
	}
	CellGrid.ClearPaint();
}

void UPaintableComponent::SetDebugDraw(bool bText, bool bCells)
{
	bDrawDebugCoverage = bText;
	bDrawDebugCells = bCells;
	UpdateTickEnabled();
}

bool UPaintableComponent::IsWorldNormalPersistent(const FVector& WorldNormal) const
{
	if (!TargetMesh) return false;
	
	// A hit normal is a true geometric normal, which the transform's inverse transpose maps: undo
	// the rotation, then multiply by the scale the inverse transpose divided out.
	const FVector LocalNormal =
		TargetMesh->GetComponentTransform().InverseTransformVectorNoScale(WorldNormal) * Scale3D;
	return IsDirectionEnabled(ClassifyPaintFaceDirection(LocalNormal));
}

UStaticMeshComponent* UPaintableComponent::FindTargetMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UStaticMeshComponent>() : nullptr;
}

uint8 UPaintableComponent::ResolveEnabledDirections() const
{
	const bool Flags[PaintFaceDirectionCount] = {bPaintFront, bPaintBack, bPaintRight, bPaintLeft, bPaintUp, bPaintDown};
	uint8 Mask = 0;
	for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
	{
		if (Flags[Direction])
		{
			Mask |= PaintDirectionBit(static_cast<EPaintFaceDirection>(Direction));
		}
	}

	if (bFloorFollowsWorldUp)
	{
		// The local direction that faces the sky the most is the floor players stand on, however
		// the actor was rolled. A sliver of a footprint (a wall's top edge) is not worth a buffer.
		const FQuat Rotation = TargetMesh->GetComponentTransform().GetRotation();
		int32 Best = 0;
		double BestDot = -2.0;
		for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
		{
			const FVector WorldAxis = Rotation.RotateVector(PaintFaceDirectionVector(static_cast<EPaintFaceDirection>(Direction)));
			const double Dot = FVector::DotProduct(WorldAxis, FVector::UpVector);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				Best = Direction;
			}
		}
		const FVector Size = MeshLocalBounds.GetSize() * Scale3D;
		const int32 Axis = Best / 2;
		const double Footprint = Size[(Axis + 1) % 3] * Size[(Axis + 2) % 3];
		if (Footprint >= UPaintSettings::Get().AutoUpMinIslandArea)
		{
			Mask |= PaintDirectionBit(static_cast<EPaintFaceDirection>(Best));
		}
	}
	return Mask;
}

FTransform UPaintableComponent::GetScaledLocalToWorld() const
{
	const FTransform& MeshTransform = TargetMesh->GetComponentTransform();
	return FTransform(MeshTransform.GetRotation(), MeshTransform.GetLocation());
}

FBox UPaintableComponent::GetScaledBounds() const
{
	return FBox(MeshLocalBounds.Min * Scale3D, MeshLocalBounds.Max * Scale3D);
}

void UPaintableComponent::OnAtlasReady(const FPaintAtlas& Atlas)
{
	// EndPlay clears these before the delegate could fire on a dead surface.
	if (!SurfaceMID || !TargetMesh)
	{
		return;
	}

	PositionMap = Atlas.PositionMap;
	EdgeFadeMap = Atlas.EdgeFadeMap;
	for (const auto& Entry : BrushMIDs)
	{
		PrimeBrushMID(*Entry.Value);
	}

	// Only the brush needs the positions, but the surface getting them too is what lets the
	// M_DebugPosition override work with zero extra plumbing.
	SurfaceMID->SetTextureParameterValue(PositionMapParam, PositionMap);
	if (EdgeFadeMap)
	{
		SurfaceMID->SetTextureParameterValue(PaintEdgeFadeParam, EdgeFadeMap);
	}

	bPaintReady = true;
	UpdateTickEnabled();
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
		// The atlas holds bounds-normalized local positions. Un-normalizing with the scaled bounds
		// puts the brush in the same scaled-local frame as the stamp, with no shader change.
		const FBox ScaledBounds = GetScaledBounds();
		BrushMID.SetTextureParameterValue(PositionMapParam, PositionMap);
		BrushMID.SetVectorParameterValue(BoundsMinParam, FLinearColor(ScaledBounds.Min));
		BrushMID.SetVectorParameterValue(BoundsSizeParam, FLinearColor(ScaledBounds.GetSize()));
	}
}

FPaintLocalStamp UPaintableComponent::ComputeLocalStamp(const FPaintSplat& Splat) const
{
	const FTransform& MeshTransform = TargetMesh->GetComponentTransform();
	const FVector AxisV = FVector::CrossProduct(FVector(Splat.Normal), FVector(Splat.AxisU));

	// Rotation and translation undone, scale kept: the scaled-local frame, where a world length
	// is still a world length on every axis.
	FPaintLocalStamp Stamp;
	Stamp.Center = MeshTransform.InverseTransformPositionNoScale(Splat.Location);
	Stamp.AxisU = MeshTransform.InverseTransformVectorNoScale(Splat.AxisU);
	Stamp.AxisV = MeshTransform.InverseTransformVectorNoScale(AxisV);
	Stamp.Normal = MeshTransform.InverseTransformVectorNoScale(Splat.Normal);
	Stamp.Radius = Splat.Radius;
	Stamp.Stretch = Splat.Stretch;
	return Stamp;
}

FBox UPaintableComponent::GetWorldBounds() const
{
	return TargetMesh ? TargetMesh->Bounds.GetBox() : FBox(ForceInit);
}
