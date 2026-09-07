#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintIslandLayout.h"
#include "Paint/PaintSplat.h"

#include "PaintableComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
struct FPaintAtlas;

/**
 * Gives its owner a paintable surface: one paint buffer per component instance that splats are
 * stamped into, a surface material that reads it back, and a coverage cell grid that scores it.
 * The surface knows nothing about brushes - a splat arrives fully resolved (FPaintSplat) and
 * brings its own brush material - so any source can paint any surface.
 *
 * A surface keeps paint only on the local face directions it has enabled - Up by default. Those
 * directions are drawn into the buffer and scored; a splat that lands on any other direction is
 * transient, and the paint subsystem shows it as a short-lived effect instead. "Floor follows
 * world up" adds whichever local direction faces the sky, so a box rolled onto its side still
 * keeps its floor without anyone ticking a box.
 *
 * The paint buffer is an atlas of planar islands, one per enabled direction: the mesh seen
 * along that axis, sized from its world footprint at the project's texel density and packed
 * into one square render target (FPaintIslandLayout). Which island a point of the surface
 * belongs to is decided by its normal's dominant local axis, so a curved surface running from
 * floor to wall simply changes island partway. A CPU bake (PaintAtlasBaker) fills a matching
 * position atlas - which point of the mesh each texel stands for - and an edge fade; the paint
 * subsystem shares those between every surface with the same mesh and layout.
 *
 * Every length the brush and the grid see is a world length: the splat is expressed in the
 * mesh's scaled-local frame (rotation and translation removed, scale kept), so the actor's scale,
 * uniform or not, stretches nothing.
 *
 * Per texel, R holds one of the PaintIdCount ids (PaintIdNone meaning "unpainted"), G the
 * accumulated paint height and B the distance to the nearest paint edge. The surface material
 * turns the id into a team look through its material layer stack (MF_PaintOverlay feeds the
 * stack input) and the height into relief. Ids must never be interpolated, so the target samples
 * with nearest filtering and the reads filter the other channels by hand.
 *
 * Writing an ID has to replace, never blend, which rules out both of the obvious draw paths:
 * translucent blend modes can never write the target's alpha, and a masked material's clip is
 * compiled out of the base pass entirely when r.EarlyZPassOnlyMaterialMasking is on (the
 * default), because the canvas path has no depth prepass to mask with. So the brush is opaque
 * and reads the previous contents itself: a splat is drawn into the subsystem's scratch buffer,
 * only over the rectangles its stamp can reach on each island, and those rectangles are copied
 * back into this surface's buffer on the render thread. The cost of a splat follows the stamp,
 * not the atlas.
 *
 * The gameplay layer rides alongside: the coverage cell grid (FPaintCellGrid), built once from
 * the mesh triangles, answers who owns how much surface. ApplySplat hands the brush and the grid
 * the same stamp, so score and picture cannot drift apart, and the render target is never read
 * back.
 *
 * Splats are never dropped: one that arrives before the atlas has baked, or while earlier ones
 * are still waiting, queues up and is drawn a few per tick. A client joining a match in progress
 * receives the whole splat history at once and the same queue spreads it over frames.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UPaintableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintableComponent();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Draws one splat into the paint buffer and marks it in the coverage grid, now or as soon as the surface is ready. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/** This surface's paint buffer. Null on a dedicated server or a surface that keeps no direction. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTextureRenderTarget2D* GetPaintRenderTarget() const { return PaintRenderTarget; }

	/** Bounds-normalized local position per texel, in the same island layout as the paint buffer. Null until baked. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTexture2D* GetPositionMap() const { return PositionMap; }

	const FPaintIslandLayout& GetIslandLayout() const { return Layout; }

	/** Directions this surface keeps paint on, resolved at BeginPlay: the six flags plus the auto-enabled floor. */
	uint8 GetEnabledDirections() const { return EnabledDirections; }

	UFUNCTION(BlueprintPure, Category = "Paint")
	bool IsDirectionEnabled(EPaintFaceDirection Direction) const { return (EnabledDirections & PaintDirectionBit(Direction)) != 0; }

	/**
	 * Whether a splat landing here with this world-space surface normal is kept and scored, or
	 * is only a passing effect. This is the one place a hit normal is turned into a direction, so
	 * the source that builds the splat and the surface that receives it cannot disagree.
	 */
	UFUNCTION(BlueprintPure, Category = "Paint")
	bool IsWorldNormalPersistent(const FVector& WorldNormal) const;

	/** Surface area each paint id owns on this mesh, in world cm^2. */
	UFUNCTION(BlueprintPure, Category = "Paint|Coverage")
	FPaintCoverage GetCoverage() const { return CellGrid.GetCoverage(); }

	/** Coverage of the part of the mesh that faces one local direction. */
	UFUNCTION(BlueprintPure, Category = "Paint|Coverage")
	FPaintCoverage GetFaceCoverage(EPaintFaceDirection Direction) const { return CellGrid.GetCoverage(Direction); }

	UFUNCTION(BlueprintCallable, Category = "Paint|Debug")
	void SetDebugDraw(bool bText, bool bCells);

	bool IsDebugTextDrawn() const { return bDrawDebugCoverage; }
	bool AreDebugCellsDrawn() const { return bDrawDebugCells; }

protected:
	/**
	 * Optional override. Leave it unset to keep whatever material the mesh already has and simply
	 * feed the paint buffers into it - that material then needs the parameters MF_PaintOverlay
	 * declares (PaintIdMap, PaintTexelSize, PaintDistRange, PositionMap, BoundsMin, BoundsSize,
	 * PaintEdgeFade and the six PaintIsland_* rectangles).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Material slot on the owner's mesh that receives the surface material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 SurfaceMaterialSlot = 0;

	/** Local +Z. The floor of anything placed the way it was modelled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintUp = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintDown = false;

	/** Local +X. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintFront = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintBack = false;

	/** Local +Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintRight = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bPaintLeft = false;

	/**
	 * Also enables whichever local direction faces world up at BeginPlay, unless its footprint
	 * is below the project's AutoUpMinIslandArea. A box rolled onto its side keeps its floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Directions")
	bool bFloorFollowsWorldUp = true;

	/**
	 * How far from a paint edge, in texels, the brush keeps an exact distance in the buffer's B
	 * channel. The reads threshold that distance for anti-aliased edges; beyond it a texel only
	 * knows which side it is on. Wider survives more minification, but costs precision in 8 bits.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning", meta = (ClampMin = "1"))
	float PaintDistanceRange = 4.0f;

	/**
	 * How many queued splats one tick draws, so a backlog (a late join replaying a whole match)
	 * is spread over frames rather than stalling one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning", meta = (ClampMin = "1"))
	int32 MaxSplatsPerTick = 16;

	/**
	 * Fraction of the splat radius that claims a cell. The stamp's main blob spans half the
	 * radius, so 0.5 follows the body; the satellite droplets reach almost to 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Coverage", meta = (ClampMin = "0.1", ClampMax = "1"))
	float CellStampFraction = 0.5f;

	/** Floating text over the mesh: total coverage and one line per local face direction. Play only: the text rides on the player's HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Debug")
	bool bDrawDebugCoverage = false;

	/** Draws every coverage cell as a slab on the surface in its paint id's debug color. Also in the editor viewport (turn Realtime on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Debug")
	bool bDrawDebugCells = false;

private:
	using FStampRects = TArray<FIntRect, TInlineAllocator<PaintFaceDirectionCount>>;

	UStaticMeshComponent* FindTargetMesh() const;
	/** Finds the mesh, resolves scale and directions and builds the grid. Everything a level designer can see without playing. */
	bool PrepareSurface();
	bool IsInEditorWorld() const;
	uint8 ResolveEnabledDirections() const;
	/** The mesh transform with its scale stripped: what maps the scaled-local frame back to the world. */
	FTransform GetScaledLocalToWorld() const;
	FBox GetScaledBounds() const;
	void OnAtlasReady(const FPaintAtlas& Atlas);
	void DrawSplat(const FPaintSplat& Splat);
	/** Atlas rectangles the stamp can touch, one per island it reaches, gutter margin included. */
	void BuildStampRects(const FPaintLocalStamp& Stamp, FStampRects& OutRects) const;
	/** Draws the primed brush over the rectangles into the scratch buffer and copies them into this surface's buffer. */
	void DrawStampRects(UMaterialInstanceDynamic& BrushMID, const FStampRects& Rects);
	void UpdateTickEnabled();
	UMaterialInstanceDynamic* GetBrushMID(UMaterialInterface* BrushMaterial);
	void PrimeBrushMID(UMaterialInstanceDynamic& BrushMID) const;
	FPaintLocalStamp ComputeLocalStamp(const FPaintSplat& Splat) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTarget;

	/** One instance per brush material that has painted this surface; splats bring their own brush. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>> BrushMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	/** Shared with every surface of the same mesh and layout; the subsystem owns them. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PositionMap;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EdgeFadeMap;

	FPaintCellGrid CellGrid;
	FPaintIslandLayout Layout;

	/** Splats waiting for the atlas, or behind others that are; drained in order by the tick. */
	TArray<FPaintSplat> PendingSplats;

	/** The mesh's own bounds, the box the position atlas is normalized to and the grid is laid over. */
	FBox MeshLocalBounds = FBox(ForceInit);

	/** Absolute world scale of the mesh, the factor between local and scaled-local. */
	FVector Scale3D = FVector::OneVector;

	/** The mesh transform the grid was built for; in the editor a moved or rescaled actor rebuilds. */
	FTransform PreparedTransform = FTransform::Identity;

	uint8 EnabledDirections = 0;

	/** True from the moment an atlas was asked for until EndPlay, so early splats know to wait. */
	bool bAtlasRequested = false;

	/** True once the atlas has arrived and every surface parameter is in place. */
	bool bPaintReady = false;
};
