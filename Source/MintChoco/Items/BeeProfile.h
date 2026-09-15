#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"
#include "Weapons/PaintDeposit.h"

#include "BeeProfile.generated.h"

class ABeeProjectile;

/**
 * 꿀벌: 가장 가까운 상대를 쫓아 날아가는 유도탄. 벽은 트레이스로 비켜 가고, 지나간 바닥에
 * 자국을 남기며, 적중하면 탄을 뿌리고 반경 안의 상대를 스턴한다. 누적 타격력이 Health에
 * 닿으면 조용히 사라지고, Lifetime이 지나면 그 자리에서 터진다. 즉발(Duration 0).
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UBeeProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee")
	TSubclassOf<ABeeProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "100", ForceUnits = "cm/s"))
	float Speed = 900.0f;

	/** 초당 최대 회전(도). 낮을수록 크게 돈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "10", ForceUnits = "deg/s"))
	float TurnRateDeg = 360.0f;

	/**
	 * 추적 곡선의 팽팽함(0~1). 꿀벌 위치에서 지금 진행 방향으로 (거리 × 이 값)만큼 나간 점을
	 * 2차 베지어의 제어점으로 삼는다. 클수록 가던 방향으로 크게 나갔다가 호를 그리며 돌아 들어오고,
	 * 0이면 곧장 목표를 향한다(예전 동작).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ClampMax = "1"))
	float CurveTension = 0.5f;

	/**
	 * 곡선 위 어느 지점의 접선을 원하는 방향으로 쓸지(0~1). 작을수록 지금 방향을 오래 유지하고
	 * 클수록 빨리 목표 쪽으로 꺾인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0.05", ClampMax = "1"))
	float CurveLookahead = 0.2f;

	/** 좌우 요동의 폭(도). 진행 방향을 이 각도만큼 사인파로 흔든다. 0이면 요동 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ClampMax = "80", ForceUnits = "deg"))
	float WobbleAmplitudeDeg = 25.0f;

	/** 좌우 요동의 빠르기(초당 왕복 횟수). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float WobbleFrequency = 1.5f;

	/**
	 * 상하 요동의 폭(도). 좌우와 다른 주파수의 사인파로 고도를 흔든다. 두 파가 합쳐져 매 순간 다른
	 * 방향(대각선, 원, 8자)으로 흔들린다. 0이면 좌우로만 흔들린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float WobblePitchAmplitudeDeg = 12.0f;

	/** 상하 요동의 빠르기(초당 왕복 횟수). 좌우와 정수비가 아니어야 궤적이 한 줄로 고정되지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float WobblePitchFrequency = 2.3f;

	/** 목표와 이 거리(cm) 안으로 들어오면 요동이 거리에 비례해 잦아든다. 마지막에는 흔들리지 않고 명중한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Curve", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WobbleSettleDistance = 300.0f;

	/** 바닥에서 이 높이 아래로 내려가면 위로 뜬다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HoverHeight = 150.0f;

	/** 앞이 막혔는지 보는 거리(cm). 속도가 빠르면 늘린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "50", ForceUnits = "cm"))
	float ProbeDistance = 200.0f;

	/** 이 시간이 지나면 그 자리에서 터진다(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "1", ForceUnits = "s"))
	float Lifetime = 8.0f;

	/** 격추까지 견디는 타격력의 합. 스나이퍼 100 한 방, 샷건 알갱이 3짜리 34발. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee", meta = (ClampMin = "1"))
	float Health = 100.0f;

	/** 바닥 자국 사이의 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Trail", meta = (ClampMin = "5", ForceUnits = "cm"))
	float MarkSpacing = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Trail")
	FPaintDeposit TrailDeposit;

	/**
	 * 상대에게 맞았을 때 그 상대의 몸 가운데에서 한 번 터지는 이펙트. 벽이나 수명으로 터질 때는
	 * 맞은 상대가 없으므로 나오지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|FX")
	TObjectPtr<UNiagaraSystem> HitFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|FX", meta = (ClampMin = "0.01"))
	float HitFXScale = 1.0f;

	/** 적중 지점에서 이 반경 안의 상대가 스턴된다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Burst", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bee|Burst")
	FPaintBurstParams Burst;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
