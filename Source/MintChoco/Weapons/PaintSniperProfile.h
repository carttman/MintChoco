#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintWeaponProfile.h"

#include "PaintSniperProfile.generated.h"

/**
 * A sniper: one hitscan ray per fully charged trigger. The ray stops at the first pawn or world
 * surface it meets, or at Range in open air. Whatever it hit, the ground under the whole ray is
 * painted as a stripe, so a shot fired upwards at 45 degrees still paints the floor beneath its
 * path; a surface at the end of the ray gets an impact stamp of its own. A pawn on the ray is the
 * victim and ends it there.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintSniperProfile : public UPaintWeaponProfile
{
	GENERATED_BODY()

public:
	UPaintSniperProfile();

	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const override;
	virtual void PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const override;
	virtual void LogUnsetReferences(const UObject* Owner) const override;

	/** What the ray leaves on the surface it ends on. Nothing when it ends on a pawn or in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper")
	FPaintDeposit Impact;

	/** What each sample of the ray leaves on the ground straight below it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper")
	FPaintDeposit Trail;

	/** How far the ray reaches when nothing stops it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Range = 10000.0f;

	/**
	 * Distance along the ray between two trail samples. Every sample that finds ground costs a
	 * stamp draw on every machine, so this spacing is what keeps one shot affordable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "10", ForceUnits = "cm"))
	float TrailSpacing = 40.0f;

	/** How far below the ray a sample looks for ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TrailDropHeight = 2000.0f;

	/** Hard cap on trail samples per shot, whatever Range / TrailSpacing says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0"))
	int32 MaxTrailSplats = 256;

	/** A ray has no impact speed of its own, so this fakes one for the brush profile's speed term. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float NominalImpactSpeed = 3000.0f;

private:
	/** Paints the ground under the ray from Muzzle along Direction for Length, skipping the shooter and the victim. */
	void PaintTrail(UWorld& World, const FVector& Muzzle, const FVector& Direction, float Length,
		const APawn* Instigator, const AActor* Victim, uint8 PaintId, int32 Seed) const;

	static void DrawTracer(const UWorld& World, const FVector& Start, const FVector& End);
};
