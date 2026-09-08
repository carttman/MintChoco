#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Paint/PaintSplat.h"

#include "PaintBrushProfile.generated.h"

class UMaterialInterface;

/**
 * What a paint source stamps with: the brush material and the rules that turn a contact into a
 * splat shape. A debug click, a paintball gun and a mop each hold one of these; the surfaces
 * they hit hold none, so any source can paint any surface with its own feel.
 *
 * BuildSplat is the only place the incidence maths lives. Every client runs it with the same
 * inputs and gets the same splat, which is what lets the result be sent instead of the hit.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintBrushProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Fills a splat from a surface hit. Seed drives the stamp's shape and, for a near-round
	 * stamp, its rotation; pass the same seed on every machine to stamp the same shape.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	FPaintSplat BuildSplat(
		const FHitResult& Hit,
		FVector IncidentVelocity,
		uint8 PaintId,
		float Volume,
		float HeightAdd,
		int32 Seed) const;

	/** Opaque brush that stamps the id, height and edge distance into a paint buffer (M_PaintBrush). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> BrushMaterial;

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
};
