#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Game/UnitMovementComponent.h"
#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"

#include "HeroLandingProfile.generated.h"

/**
 * 히어로 랜딩: 점프대 높이만큼 뜨고, 공중에서 착지점을 고른 뒤 내리꽂힌다. 착지 지점에서 탄을
 * 뿌리고 반경 안의 상대를 밀어내며 스턴한다. Duration은 안전 상한(보통 6초)이고 착지하면 그
 * 전에 끝난다. 움직임 자체는 UUnitMovementComponent의 단계 기계가 맡는다(Landing).
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UHeroLandingProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	/** 상승·정지·내리꽂기의 수치. 어빌리티가 시작할 때 양쪽 무브먼트에 같은 값을 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	FHeroLandingParams Landing;

	/** 착지 지점에서 이 반경 안의 상대가 밀리고 스턴된다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunRadius = 400.0f;

	/** 착지 때 뿌리는 탄. PaintId와 Seed는 런타임에 채워진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	FPaintBurstParams Burst;

	/** 소유 클라이언트에만 보이는 착지점 표시. 매 틱 조준점으로 옮겨지고 내리꽂기부터 고정된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	TSubclassOf<AActor> AimMarkerClass;

	/**
	 * 사용하는 순간 발밑에서 한 번 터지는 이펙트. 붙이지 않고 그 자리에 두므로 사용자가
	 * 솟아올라도 출발한 자리에 남는다. ItemProfile의 ActivateFX와 달리 일회성이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX")
	TObjectPtr<UNiagaraSystem> StartFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX", meta = (ClampMin = "0.01"))
	float StartFXScale = 1.0f;

	/** 착지하는 순간 착지 지점에서 한 번 터지는 이펙트. 탄을 뿌리는 그 지점이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX")
	TObjectPtr<UNiagaraSystem> LandFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX", meta = (ClampMin = "0.01"))
	float LandFXScale = 1.0f;

	/** 공중에 멈춰 조준하는 동안 발밑에 떠 있는 이펙트. 루프하는 것을 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX")
	TObjectPtr<UNiagaraSystem> HoverFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX", meta = (ClampMin = "0.01"))
	float HoverFXScale = 1.0f;

	/** 캡슐 중심에서의 높이(cm). 음수면 발밑이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX", meta = (ForceUnits = "cm"))
	float HoverFXZOffset = -90.0f;

	/**
	 * 내리꽂기가 시작되고 이만큼 더 있다가 끈다(초). 총 재생 길이는 HoverTime + 이 값이라,
	 * 호버 시간을 바꾸면 알아서 따라간다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|FX", meta = (ClampMin = "0", ForceUnits = "s"))
	float HoverFXStopDelay = 0.25f;

	/**
	 * 사용하는 순간 카메라를 월드 기준으로 이만큼 올린다(cm). 내리꽂기가 시작되면 원래
	 * 높이로 돌아온다. 0이면 카메라를 건드리지 않는다. 붐의 카메라 랙이 살아 있으므로
	 * 실제로는 아주 짧게 미끄러지듯 올라간다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding|Camera", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CameraRiseOffset = 400.0f;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
