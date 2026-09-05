#pragma once

#include "CoreMinimal.h"

#include "PaintDeposit.generated.h"

class UPaintBrushProfile;
class UWorld;
struct FHitResult;
struct FPaintSplat;

/**
 * What one contact leaves on the surface: the stamp shape and how much of it. A paintball and a
 * brush head both carry one, so every kind of profile deposits through the same three knobs.
 */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintDeposit
{
	GENERATED_BODY()

	/** How a hit becomes a splat: brush material plus the speed and incidence tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UPaintBrushProfile> BrushProfile;

	/** Scales the splat's area, so its radius follows the square root: four times the volume is twice the radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0"))
	float SplatVolume = 1.0f;

	/** Height fraction one splat deposits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightAdd = 0.35f;

	bool CanPaint() const { return BrushProfile != nullptr; }

	/** Builds the splat for a contact. Requires BrushProfile. */
	FPaintSplat BuildSplat(const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const;

	/**
	 * Builds the splat and hands it to the world. Returns false, painting nothing, when the hit is
	 * not on a paintable surface, the world has no paint subsystem, or BrushProfile is unset.
	 */
	bool ApplyHit(UWorld* World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const;

	static bool IsPaintable(const FHitResult& Hit);
};
