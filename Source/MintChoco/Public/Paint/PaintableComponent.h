#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Paint/PaintSplat.h"

#include "PaintableComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UTextureRenderTarget2D;

/**
 * Gives its owner a paint layer: one render target per component instance, a brush material
 * drawn into it, and a surface material that reads it back.
 *
 * The render target is a team-ID buffer, not a color buffer: R8, 0 = unpainted, 1..255 = owning
 * team, decoded to a color through a palette in the surface material. IDs must never be
 * interpolated, so the target samples with nearest filtering.
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
	 * Fills in a splat from a trace or collision hit against this component's owner.
	 *
	 * The UV lookup lives here rather than in the caller because the UV layout is a property of
	 * the surface, not of whatever threw paint at it. A click trace, a paintball and a mop all
	 * arrive with an FHitResult and should not each need to know the channel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	bool BuildSplatFromHit(
		const FHitResult& Hit,
		FVector IncidentVelocity,
		uint8 TeamId,
		float Volume,
		FPaintSplat& OutSplat) const;

	/** Draws one splat into the render target. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

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

protected:
	/** Square resolution of the paint render target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 RenderTargetResolution = 1024;

	/**
	 * UV channel the paint is addressed in.
	 *
	 * Channel 0 of the engine's basic shapes maps every face onto the same 0-1 square, so
	 * painting one face of a cube would paint all six. The lightmap channel is a real
	 * non-overlapping unwrap, which is what painting needs. This whole property disappears once
	 * painting moves to a world-position unwrap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 PaintUVChannel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> BrushMaterial;

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

	/** Radius in UV units for a splat of unit volume arriving at zero speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float BaseRadius = 0.05f;

	/** UV radius added per cm/s of impact speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float RadiusPerSpeed = 0.000005f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float MaxRadius = 0.25f;

	/** Upper bound on 1 / cos(incidence). Without it a grazing hit stretches to infinity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Tuning")
	float MaxStretch = 3.0f;

private:
	UMeshComponent* FindTargetMesh() const;
	UTextureRenderTarget2D* CreateIdBuffer();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTargets[2];

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrushMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	int32 FrontBufferIndex = 0;
};
