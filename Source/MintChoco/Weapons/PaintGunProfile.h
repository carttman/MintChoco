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
	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const override;
	virtual void PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const override;

	/**
	 * Flies the centre pellet, spread aside, from the muzzle with the ball's gravity (both phases
	 * when DropAfter is set) until it hits something or the ball's life span runs out. OutImpact is
	 * the hit, or where the ball would die in the air.
	 */
	virtual bool PredictImpact(const FPaintFireContext& Context, FVector& OutAimPoint, FVector& OutImpact) const override;
	virtual void LogUnsetReferences(const UObject* Owner) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	TObjectPtr<UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	TObjectPtr<UPaintScatterProfile> Scatter;

	/** How far the view ray looks for the aim point the barrel converges on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AimTraceDistance = 10000.0f;

	/**
	 * 한 발의 산탄이 내는 착탄음을 첫 탄 하나로 줄인다.
	 *
	 * 펠릿이 거의 동시에 닿으므로 탄마다 울리면 같은 소리가 겹쳐 지저분해진다. 켜면 첫 탄만
	 * 소리를 내고 나머지는 조용히 칠하기만 한다. 칠하는 양과 이펙트는 그대로다.
	 *
	 * 기본은 꺼짐이라 이 값을 넣지 않은 기존 총은 전과 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun")
	bool bImpactSoundOncePerShot = false;

private:
	/**
	 * The player aims with the camera, not the barrel: finds what the crosshair rests on
	 * (OutAimPoint) and the unit direction from the muzzle that converges on it. A target closer
	 * than the muzzle or behind it falls back to the view direction.
	 */
	void ComputeAim(const FPaintFireContext& Context, FVector& OutAimPoint, FVector& OutDirection) const;

	/** Scatters the shot and launches one ball per pellet. Returns true when at least one flew. */
	bool Launch(UWorld& World, APawn* Instigator, const FPaintShot& Shot, bool bCosmetic) const;
};
