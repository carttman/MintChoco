#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplat.h"

#include "PaintableComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPositionMapBaker;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * Gives its owner a paint layer: one render target per component instance, a brush material
 * drawn into it, and a surface material that reads it back.
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
 * The gameplay layer rides alongside: a coverage cell grid (FPaintCellGrid) built once from the
 * mesh triangles answers who owns how much surface. ApplySplat hands the brush and the grid the
 * same local stamp, so score and picture cannot drift apart, and the render target is never read.
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

	/**
	 * Fills in a splat from a trace or collision hit. A click trace, a paintball and a mop all
	 * arrive with an FHitResult and produce this same struct; only the values differ.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	FPaintSplat BuildSplatFromHit(
		const FHitResult& Hit,
		FVector IncidentVelocity,
		uint8 PaintId,
		float Volume) const;

	/** Draws one splat into the render target. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	/**
	 * Applies one splat to every paintable surface within the splat's world radius. The
	 * world-space brush makes this trivial: every component tests its own texels against the
	 * same sphere, so a splat landing near an actor boundary simply paints both sides.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint", meta = (WorldContext = "WorldContextObject"))
	static void ApplySplatInRadius(const UObject* WorldContextObject, const FPaintSplat& Splat, float WorldRadius);

	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/**
	 * Turns a contact event into drawing parameters. All of the incidence-angle behaviour
	 * lives here, so a paintball and a mop stroke go through exactly the same maths.
	 */
	UFUNCTION(BlueprintPure, Category = "Paint")
	FPaintSplatShape ComputeSplatShape(const FPaintSplat& Splat) const;

	/** The target currently holding the paint. The other one is scratch for the next splat. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTextureRenderTarget2D* GetPaintRenderTarget() const { return PaintRenderTargets[FrontBufferIndex]; }

	/** Bounds-normalized local position per texel. Null until the baker has delivered. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	UTextureRenderTarget2D* GetPositionRenderTarget() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> BrushMaterial;

	/**
	 * Unlit material whose WPO flattens the mesh into its UV layout while the emissive outputs
	 * the pre-offset local position, normalized to the object bounds (M_PaintUnwrap).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> UnwrapMaterial;

	/**
	 * World-space size of the plane the mesh unwraps onto during the bake. The value itself is
	 * arbitrary; it only has to match the capture's ortho width, which BakePositionMap guarantees.
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

	/**
	 * Unlit material that writes, per texel, how close the unwrap island edge is (M_PaintEdgeFade).
	 * Displacement is scaled by it: at a hard edge the neighbouring faces rise along different
	 * vertex normals, so any height left there tears the mesh open.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> EdgeFadeMaterial;

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

	/** Draws every coverage cell as a box in its paint id's debug color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Debug")
	bool bDrawDebugCells = false;

	/** Radius in cm for a splat of unit volume arriving at zero speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float BaseRadius = 25.0f;

	/** cm of radius added per cm/s of impact speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float RadiusPerSpeed = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float MaxRadius = 120.0f;

	/** Upper bound on 1 / cos(incidence). Without it a grazing hit stretches to infinity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float MaxStretch = 3.0f;

	/**
	 * Fraction of the stretch-added radius the splat center slides along the tangent. 0 keeps
	 * the ellipse centered on the contact; 1 keeps the near edge pinned there instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float CenterShiftScale = 0.5f;

	/**
	 * Below this stretch the stamp is visually round, so aligning it to the incident tangent
	 * just repeats one orientation every click; such splats spin from the seed instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float MinAlignedStretch = 1.2f;

private:
	static UTextureRenderTarget2D* CreateIdBuffer(UObject* Outer, int32 Resolution);
	static UTextureRenderTarget2D* CreateFadeBuffer(UObject* Outer, int32 Resolution);

	UStaticMeshComponent* FindTargetMesh() const;
	void OnPositionMapBaked(UTextureRenderTarget2D* PositionMap);
	void BakeEdgeFade(UTextureRenderTarget2D* PositionMap);
	void BuildCellGrid();
	FPaintLocalStamp ComputeLocalStamp(const FPaintSplat& Splat, const FPaintSplatShape& Shape) const;
	void DrawDebugCoverage() const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTargets[2];

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> EdgeFadeRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EdgeFadeMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrushMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPositionMapBaker> PositionBaker;

	FPaintCellGrid CellGrid;

	/** The mesh's own bounds, the box the position map is normalized to and the grid is laid over. */
	FBox MeshLocalBounds = FBox(ForceInit);

	int32 FrontBufferIndex = 0;

	/** True once the position map has baked and every MID parameter is in place. */
	bool bPaintReady = false;
};
