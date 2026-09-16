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
	 * 펠릿을 조준선을 따라 서로 다른 거리에 떨어뜨린다.
	 *
	 * 끄면 펠릿은 무언가에 닿을 때까지 그대로 날아가므로 착탄 자국이 전부 먼 곳에 몰린다.
	 * 켜면 차지샷 볼리와 같은 방식으로(UPaintballProfile::Launch의 DropAfterOverride) 펠릿마다
	 * 꺾이는 시점을 달리해, 사수 앞쪽부터 경로를 따라 자국이 나뉘어 찍힌다.
	 *
	 * 기본은 꺼짐이라 이 값을 넣지 않은 기존 총은 전과 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|PelletDrop")
	bool bStaggerPelletDrop = false;

	/** 부채꼴 **바깥쪽** 펠릿이 떨어지는 거리(cm). 가장 가까이 떨어지는 쪽이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|PelletDrop",
		meta = (ClampMin = "0", ForceUnits = "cm", EditCondition = "bStaggerPelletDrop"))
	float PelletDropNearDistance = 400.0f;

	/**
	 * 부채꼴 **가운데** 펠릿이 떨어지는 거리(cm). 가장 멀리 나가는 쪽이다.
	 *
	 * 가운데가 멀고 바깥이 가까우므로 바닥에 삼각형으로 퍼진 자국이 남는다. 순서대로
	 * 늘어놓으면 왼쪽 가까이에서 오른쪽 멀리로 비스듬한 줄이 되어 부채꼴처럼 보이지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|PelletDrop",
		meta = (ClampMin = "0", ForceUnits = "cm", EditCondition = "bStaggerPelletDrop"))
	float PelletDropFarDistance = 1600.0f;

	/**
	 * 목표 지점보다 이만큼 앞에서 꺾기 시작한다(cm).
	 *
	 * 탄은 꺾인 뒤에도 앞으로 나아가므로 목표 바로 위에서 꺾으면 그만큼 지나친다. 차지샷 볼리의
	 * VolleyDropLead와 같은 뜻이다. 이 값보다 가까운 목표는 전부 즉시 꺾여 한자리에 뭉치므로,
	 * 가까이 깔고 싶으면 이 값을 줄이고 탄의 DropGravityScale을 함께 올려야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun|PelletDrop",
		meta = (ClampMin = "0", ForceUnits = "cm", EditCondition = "bStaggerPelletDrop"))
	float PelletDropLead = 300.0f;

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

	/**
	 * 이 펠릿이 몇 초 직진한 뒤 꺾일지. 끄면 -1(프로필의 DropAfter를 그대로 쓴다)을 돌려준다.
	 * 부채꼴 가운데일수록 오래 직진해 멀리 떨어진다.
	 */
	float ComputePelletDropAfter(int32 Pellet, int32 PelletCount) const;
};
