#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintWeaponProfile.h"

#include "PaintStrokeProfile.generated.h"

/**
 * A brush head or mop: stamps the surface under the crosshair as long as it is within reach of
 * the weapon, spacing the stamps along the stroke. Nothing flies, so it carries its deposit
 * directly instead of a paintball.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintStrokeProfile : public UPaintWeaponProfile
{
	GENERATED_BODY()

public:
	UPaintStrokeProfile();

	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke) const override;
	virtual void LogUnsetReferences(const UObject* Owner) const override;

	/** What each stamp leaves on the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke")
	FPaintDeposit Deposit;

	/** Farthest the aimed surface may be from the muzzle and still get painted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Reach = 150.0f;

	/**
	 * Distance the aim point must travel before the stroke deposits the next stamp. Every stamp
	 * costs a full-target draw, so this spacing is what keeps a held stroke affordable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StrokeSpacing = 15.0f;

	/** A brush has no impact speed of its own, so this fakes one for the brush profile's speed term. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stroke", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float NominalImpactSpeed = 800.0f;
};
