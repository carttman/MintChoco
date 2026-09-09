#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProfile.h"
#include "Weapons/PaintDeposit.h"

#include "SpeedStarProfile.generated.h"

/**
 * 스피드 스타: 잠시 고정 속도로 달리며 지나간 바닥을 자기 색으로 칠한다.
 *
 * 속도 값은 여기 없다. 이동 속도는 서버와 클라이언트가 같은 무브에서 같은 값을 써야
 * 하므로 UUnitMovementComponent::SpeedBoostSpeed 하나가 진실이고, 이 프로필은 그
 * 부스트를 켜는 아이템이라는 것만 말한다(GrantsSpeedBoost).
 */
UCLASS(BlueprintType)
class MINTCHOCO_API USpeedStarProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	/** 자국 사이의 거리(cm). 1500 cm/s × 3 s = 45 m를 40 cm로 나누면 약 112개. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedStar", meta = (ClampMin = "5", ForceUnits = "cm"))
	float MarkSpacing = 40.0f;

	/** 자국 하나가 바닥에 남기는 것. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedStar")
	FPaintDeposit TrailDeposit;

	/**
	 * 자국이 달리는 방향으로 늘어나는 배율. 1이면 수직으로 떨어진 둥근 자국, 2.5면 진행 방향으로 2.5배 길게
	 * 번진다(브러시 프로필의 MaxStretch가 상한). 자국을 비스듬한 충돌로 만들어 브러시가 알아서 늘린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedStar", meta = (ClampMin = "1", ForceUnits = "x"))
	float TrailStretch = 2.5f;

	/** 효과가 끝난 뒤 무지개 자국이 팀 색으로 돌아오는 데 걸리는 시간(초). 효과 중에는 자국을 다른 팀이 덮지 못한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedStar", meta = (ClampMin = "0", ForceUnits = "s"))
	float PaintFadeDuration = 2.0f;

	virtual bool GrantsSpeedBoost() const override { return true; }
	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
