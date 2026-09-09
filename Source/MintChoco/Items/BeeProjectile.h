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

	/** 서버 전용. 발사 순간 고른 상대. 없으면 직진한다. */
	void SetTarget(AUnit* InTarget);

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
	FVector LastMark = FVector::ZeroVector;
	float Damage = 0.0f;
	FTimerHandle LifeTimer;
};
