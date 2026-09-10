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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0", ForceUnits = "x"))
	float SplatVolume = 1.0f;

	/** Height fraction one splat deposits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightAdd = 0.35f;

	/**
	 * How hard this contact strikes a paint hit receiver (a balloon with 100 health, say). A game
	 * number, separate from SplatVolume so the size of the mark and the balance of the hit can be
	 * tuned apart: a sniper impact of 100 pops a balloon in one, a shotgun pellet of 3 needs a few shots.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0"))
	float HitPower = 1.0f;

	bool CanPaint() const { return BrushProfile != nullptr; }

	/** Height fraction one splat deposits, 0 to 1. */
	float GetHeightAdd() const { return HeightAddPercent * 0.01f; }

	/** Builds the splat for a contact. Requires BrushProfile. */
	FPaintSplat BuildSplat(const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const;

	/**
	 * Builds the splat and hands it to the world. Returns false, painting nothing, when the hit is
	 * not on a paintable surface, the world has no paint subsystem, or BrushProfile is unset.
	 * A hit actor that is a paint hit receiver is struck with HitPower first, whether or not it
	 * is also painted.
	 */
	bool ApplyHit(UWorld* World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const;

	/** Strikes the hit actor if it is a paint hit receiver. Returns true when something received the hit. */
	bool StrikeReceiver(const FHitResult& Hit, uint8 PaintId) const;

  	/** The hit actor owns a paint buffer. */
	static bool IsPaintable(const FHitResult& Hit);

	/**
	 * The hit lands on something a splat may be submitted for: a paintable surface, or a static
	 * mesh that only ever shows a passing effect. Moving geometry is out, since the effect stays
	 * where the surface was.
	 */
	static bool ReceivesSplat(const FHitResult& Hit);

	/**
	 * The hit surface owns a paint buffer and keeps paint facing this way, so a splat submitted
	 * here stays. False means the contact can only ever be a passing effect - useful to a producer
	 * that would rather skip such a contact than spawn one effect actor per sample.
	 */
	static bool KeepsPaint(const FHitResult& Hit);

	/**
	 * Asks the hit surface whether it keeps paint facing this way and flags the splat transient
	 * when it does not, or when the hit actor has no paint buffer at all. Every producer of a
	 * splat calls this before submitting, so the decision is made once, on the authority, where
	 * the hit actor is known.
	 */
	static void MarkTransience(FPaintSplat& Splat, const FHitResult& Hit);

private:
	/** Percent of the full paint height one splat deposits. Read it through GetHeightAdd. */
	UPROPERTY(EditAnywhere, Category = "Paint", meta = (ClampMin = "0", ClampMax = "100", ForceUnits = "%"))
	float HeightAddPercent = 35.0f;
};
