#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplat.h"

#include "PaintableComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaintMapBaker;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * Gives its owner a paintable surface: one paint buffer per component instance that splats are
 * stamped into, a surface material that reads it back, and a coverage cell grid that scores it.
 * The surface knows nothing about brushes - a splat arrives fully resolved (FPaintSplat) and
 * brings its own brush material - so any source can paint any surface.
 *
 * The render target is a paint buffer, not a color buffer. Per texel, R holds one of the
 * PaintIdCount ids (PaintIdNone meaning "unpainted"), G the accumulated paint height and B the
 * distance to the nearest paint edge. The surface material turns the id into a team look
 * through its material layer stack (MF_PaintOverlay feeds the stack input) and the height into
 * relief. Ids must never be interpolated, so the target samples with nearest filtering and the
 * reads filter the other channels by hand.
 *
 * Writing an ID has to replace, never blend, which rules out both of the obvious draw paths:
 * translucent blend modes can never write the target's alpha, and a masked material's clip is
 * compiled out of the base pass entirely when r.EarlyZPassOnlyMaterialMasking is on (the
 * default), because the depth prepass is expected to have done the masking - and the canvas
 * path that DrawMaterialToRenderTarget uses has no depth prepass. So the brush is opaque and
 * covers the whole target, and the previous contents are preserved by reading them from a
 * second target: the brush outputs its ID inside the splat and the previous ID everywhere else,
 * and the two targets swap after each draw.
 *
 * Two render targets per instance is deliberate. Sharing them between actors is the first thing
 * that breaks once a second paintable surface exists in the level.
 *
 * The gameplay layer rides alongside: the coverage cell grid (FPaintCellGrid), built once from
 * the mesh triangles, answers who owns how much surface. ApplySplat hands the brush and the grid
 * the same local stamp, so score and picture cannot drift apart, and the render target is never
 * read back.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UPaintableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Draws one splat into the paint buffer and marks it in the coverage grid. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/** The target currently holding the paint. The other one is scratch for the next splat. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTextureRenderTarget2D* GetPaintRenderTarget() const { return PaintRenderTargets[FrontBufferIndex]; }

	/** Bounds-normalized local position per texel. Null until the baker has delivered. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTextureRenderTarget2D* GetPositionRenderTarget() const { return PositionMap; }

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
	/** Square resolution of the paint render target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 RenderTargetResolution = 1024;

	/**
	 * World-space size of the plane the mesh unwraps onto during the bake. The value itself is
	 * arbitrary; it only has to match the capture's ortho width, which the baker guarantees.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	float UnwrapPlaneSize = 1000.0f;

	/**
	 * Optional override. Leave it unset to keep whatever material the mesh already has and simply
	 * feed the paint buffers into it - that material then needs the parameters MF_PaintOverlay
	 * declares (PaintIdMap, PaintTexelSize, PaintDistRange, PositionMap, BoundsSize, PaintEdgeFade).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Material slot on the owner's mesh that receives the surface material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 SurfaceMaterialSlot = 0;

	/** Width of the displacement fade at island edges, in paint texels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning", meta = (ClampMin = "1"))
	float EdgeFadeTexels = 8.0f;

	/**
	 * Two neighbouring texels whose baked positions differ by more than this fraction of the
	 * mesh bounds belong to different unwrap islands, so the fade treats the gap as an edge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning", meta = (ClampMin = "0.001", ClampMax = "1"))
	float EdgeFadeSeamFraction = 0.05f;

	/**
	 * How far from a paint edge, in texels, the brush keeps an exact distance in the buffer's B
	 * channel. The reads threshold that distance for anti-aliased edges; beyond it a texel only
	 * knows which side it is on. Wider survives more minification, but costs precision in 8 bits.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning", meta = (ClampMin = "1"))
	float PaintDistanceRange = 4.0f;

	/**
	 * World-space edge of one coverage cell. Cells are the gameplay layer's unit of ownership;
	 * the paint buffer keeps its texel resolution regardless.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Coverage", meta = (ClampMin = "5"))
	float CellSize = 25.0f;

	/**
	 * Fraction of the splat radius that claims a cell. The stamp's main blob spans half the
	 * radius, so 0.5 follows the body; the satellite droplets reach almost to 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Coverage", meta = (ClampMin = "0.1", ClampMax = "1"))
	float CellStampFraction = 0.5f;

	/** Floating text over the mesh: total coverage and one line per local face direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Debug")
	bool bDrawDebugCoverage = false;

	/** Draws every coverage cell as a slab on the surface in its paint id's debug color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Debug")
	bool bDrawDebugCells = false;

private:
	static UTextureRenderTarget2D* CreateIdBuffer(UObject* Outer, int32 Resolution);

	UStaticMeshComponent* FindTargetMesh() const;
	float GetUniformScale() const;
	void OnMapsBaked(UTextureRenderTarget2D* InPositionMap, UTextureRenderTarget2D* EdgeFadeMap);
	UMaterialInstanceDynamic* GetBrushMID(UMaterialInterface* BrushMaterial);
	void PrimeBrushMID(UMaterialInstanceDynamic& BrushMID) const;
	FPaintLocalStamp ComputeLocalStamp(const FPaintSplat& Splat) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTargets[2];

	/** One instance per brush material that has painted this surface; splats bring their own brush. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>> BrushMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPaintMapBaker> MapBaker;

	/** Kept after the bake so a brush instance created later can still be primed with it. */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PositionMap;

	FPaintCellGrid CellGrid;

	/** The mesh's own bounds, the box the position map is normalized to and the grid is laid over. */
	FBox MeshLocalBounds = FBox(ForceInit);

	int32 FrontBufferIndex = 0;

	/** True once the maps have baked and every surface parameter is in place. */
	bool bPaintReady = false;
};
