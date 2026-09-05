#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintWeaponProfile.h"

#include "PaintGunProfile.generated.h"

class UPaintScatterProfile;
class UPaintballProfile;

/**
 * A gun: a paintball, a scatter pattern and a cadence. The two parts are separate assets so a
 * designer can pair any ball with any spread, or tune one side without touching the other.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintGunProfile : public UPaintWeaponProfile
{
	GENERATED_BODY()

public:
	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke) const override;
	virtual void LogUnsetReferences(const UObject* Owner) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	TObjectPtr<UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	TObjectPtr<UPaintScatterProfile> Scatter;

	/** How far the view ray looks for the aim point the barrel converges on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AimTraceDistance = 10000.0f;
};
