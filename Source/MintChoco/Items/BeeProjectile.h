#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProjectile.h"
#include "Weapons/PaintHitReceiver.h"

#include "BeeProjectile.generated.h"

class UBeeProfile;
class USphereComponent;

/** 꿀벌의 조종 규칙. 월드 없이 테스트한다. */
struct MINTCHOCO_API FBeeSteering
{
	/**
	 * 후보 방향. 순서가 곧 우선순위: 원하는 방향, 그 좌우 30·60·90도, 위로 45도, 바로 위.
	 * 벽 앞에서 옆으로 비켜 가고, 옆도 막히면 넘어간다.
	 */
	static void BuildCandidates(const FVector& Desired, TArray<FVector>& OutCandidates);

	/** Blocked[i]가 거짓인 첫 후보. 전부 막히면 바로 위. */
	static FVector Choose(const TArray<FVector>& Candidates, const TArray<bool>& Blocked);

	/** Current에서 Desired 쪽으로 최대 MaxAngleDeg만 돈 단위 벡터. */
	static FVector TurnTowards(const FVector& Current, const FVector& Desired, float MaxAngleDeg);

	/**
	 * 고도 오차(목표 고도 − 현재 고도, cm)를 방향의 Z 성분으로. Scale cm 차이에서 최대 기울기
	 * MaxRise에 닿고 그 안에서는 비례한다. 문턱값이 아니라 연속값이라 매 틱 뒤집히지 않는다.
	 */
	static float VerticalComponent(float AltitudeError, float Scale, float MaxRise = 0.7f);

	/** 수평 단위 방향과 Z 성분을 합친 단위 벡터. Flat이 0이면 위/아래만. */
	static FVector Combine(const FVector& Flat, float Vertical);

	/**
	 * 요와 피치를 따로 제한해 돈다. 수평 방향은 수평면 안에서 최대 MaxAngleDeg, Z 성분은 그 각도만큼만
	 * 바뀐다. 3D 최단 호로 돌리면 크게 꺾이면서 내려갈 때 호가 수직 아래를 지나 바닥에 박힌다.
	 */
	static FVector TurnTowardsSplit(const FVector& Current, const FVector& WantedFlat, float WantedVertical, float MaxAngleDeg);

	/**
	 * 지면 여유(cm)에 따라 허용되는 최대 하강 성분. 여유가 Scale이면 MaxDive, 0이면 0. 회전 제한이
	 * 따라잡기 전에 바닥에 닿지 않도록 낮을수록 얕게 내려간다.
	 */
	static float MaxDescent(float Clearance, float Scale, float MaxDive = 0.7f);
};

/**
 * 날아가는 꿀벌. 서버가 매 틱 조종하고 이동은 복제된다.
 *
 * 콜리전은 둘이다. 뿌리 구는 월드에 막히고 폰과 겹치되 페인트탄은 무시해서 탄에 부딪혀 멈추는
 * 일이 없고, 자식 Shell은 페인트탄만 막아 탄이 여기에 맞으면 ApplyHit → IPaintHitReceiver로
 * 타격력이 들어온다(풍선과 같은 경로). 자기 팀 탄은 무시한다.
 */
UCLASS()
class MINTCHOCO_API ABeeProjectile : public AItemProjectile, public IPaintHitReceiver
{
	GENERATED_BODY()

public:
	ABeeProjectile();

	/** 서버 전용. Init 뒤에. */
	void SetProfile(const UBeeProfile* InProfile);

	/** 서버 전용. 발사 순간 고른 상대. 없으면 BeginPlay에서 가장 가까운 상대를 고르고, 그래도 없으면 직진한다. */
	void SetTarget(AUnit* InTarget);

	/** Exclude(사용자)와 Team의 아군을 뺀 가장 가까운 유닛. 팀이 없으면 Exclude만 뺀다. 없으면 nullptr. */
	static AUnit* FindNearestOpponent(const UWorld& World, const FVector& From, int32 Team, const AActor* Exclude);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	//~ IPaintHitReceiver
	virtual void ReceivePaintHit_Implementation(float HitPower, uint8 PaintId, const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Bee")
	float GetDamageFraction() const;

protected:
	virtual void HandleUnitOverlap(AUnit& Unit) override;
	virtual void HandleWorldHit(const FHitResult& Hit) override;
	virtual void OnDetonate() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bee")
	TObjectPtr<USphereComponent> Shell;

private:
	void Steer(float DeltaTime);
	void DropTrail();
	bool IsBlocked(const FVector& Direction) const;
	void Expire();

	UPROPERTY(Transient)
	TObjectPtr<const UBeeProfile> Profile;

	TWeakObjectPtr<AUnit> Target;

	/**
	 * 실제로 부딪힌 상대. 쫓던 Target과 다를 수 있고(다른 상대에 먼저 닿는다), 벽이나 수명으로
	 * 터질 때는 비어 있다. OnDetonate가 "상대에게 맞았는지"를 이것으로 가른다.
	 */
	TWeakObjectPtr<AUnit> StruckUnit;

	FVector LastMark = FVector::ZeroVector;
	float Damage = 0.0f;
	FTimerHandle LifeTimer;

	/** 고른 회피 방향과 그것을 유지할 남은 시간. 매 틱 다른 후보로 갈아타면 지그재그가 된다. */
	FVector AvoidanceDirection = FVector::ZeroVector;
	float AvoidanceTimeLeft = 0.0f;
};
