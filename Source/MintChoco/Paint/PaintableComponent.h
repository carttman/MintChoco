#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
 * The render target is a paint-id buffer, not a color buffer: R8 holding one of the
 * PaintIdCount ids per texel, PaintIdNone meaning "unpainted". The surface material decodes
 * each id to its designer-assigned color (MF_PaintOverlay's per-id parameters). Ids must never
 * be interpolated, so the target samples with nearest filtering.
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
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UPaintableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	 * feed the paint render target into it - that material then needs a PaintRT texture parameter,
	 * which is what MF_PaintOverlay provides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Material slot on the owner's mesh that receives the surface material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 SurfaceMaterialSlot = 0;

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

private:
	static UTextureRenderTarget2D* CreateIdBuffer(UObject* Outer, int32 Resolution);

	UStaticMeshComponent* FindTargetMesh() const;
	void OnPositionMapBaked(UTextureRenderTarget2D* PositionMap);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTargets[2];

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrushMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPositionMapBaker> PositionBaker;

	int32 FrontBufferIndex = 0;

	/** True once the position map has baked and every MID parameter is in place. */
	bool bPaintReady = false;
};
