#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProjectile.h"
#include "Weapons/PaintHitReceiver.h"

#include "BeeProjectile.generated.h"

class UBeeProfile;
class USkeletalMeshComponent;
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

	/** 2차 베지어 P0-P1-P2의 T 지점 접선(정규화 안 함): 2(1−T)(P1−P0) + 2T(P2−P1). */
	static FVector BezierTangent(const FVector& P0, const FVector& P1, const FVector& P2, float T);

	/**
	 * 목표를 향해 호를 그리는 수평 단위 방향. 제어점을 지금 진행 방향(CurrentFlat) 위에 두므로
	 * 곡선의 출발 접선이 지금 방향과 같고, Lookahead 지점의 접선을 원하는 방향으로 쓴다.
	 * Tension 0이면 목표를 곧장 향한다. 목표와 겹치면 CurrentFlat.
	 */
	static FVector CurveHeading(const FVector& Location, const FVector& CurrentFlat, const FVector& TargetLocation, float Tension, float Lookahead);

	/**
	 * 좌우 요동의 요 오프셋(도). AmplitudeDeg × sin(2π·Frequency·Time)에 거리 비율을 곱한다:
	 * DistanceToTarget이 SettleDistance 안이면 그 비율만큼 줄어 마지막에는 흔들리지 않는다.
	 */
	static float WobbleYawDeg(float Time, float AmplitudeDeg, float FrequencyHz, float DistanceToTarget, float SettleDistance);

	/**
	 * 상하 요동의 피치 오프셋(도). 같은 식이되 주파수를 따로 받는다. 요와 피치를 다른 주파수로 흔들면
	 * 합성 방향이 매 순간 달라져 좌우 한 줄이 아니라 사방으로 요동친다.
	 */
	static float WobblePitchDeg(float Time, float AmplitudeDeg, float FrequencyHz, float DistanceToTarget, float SettleDistance);

	/** 피치 오프셋(도)을 방향 벡터의 Z 성분 증분으로. 고도 성분(VerticalComponent)에 더해 쓴다. */
	static float PitchToVertical(float PitchDeg) { return FMath::Sin(FMath::DegreesToRadians(PitchDeg)); }
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

	virtual void PostInitializeComponents() override;
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

	/**
	 * 꿀벌 몸체. 스켈레탈 메시라 부모의 스태틱 Mesh 대신 이것을 그리고, 클라이언트의 넷 보간도
	 * 이것을 끌어간다(부모는 Mesh를 보간 대상으로 걸고, 여기서 바꿔 단다). BP_Bee가 메시를 정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bee")
	TObjectPtr<USkeletalMeshComponent> Body;

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

	/** 요동 사인파의 시계(초). 서버만 조종하므로 서버 시각이면 된다. */
	float WobbleTime = 0.0f;

	/** 이번 틱의 상하 요동(도). 수평 방향을 정할 때 계산해 두고 고도를 정할 때 더한다. */
	float WobblePitch = 0.0f;
};
