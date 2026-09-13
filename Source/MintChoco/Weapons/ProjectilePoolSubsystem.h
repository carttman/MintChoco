#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"

#include "ProjectilePoolSubsystem.generated.h"

class APaintProjectile;
class APawn;
class UPaintballProfile;

/** 한 클래스의 놀고 있는 공들. TMap의 값으로 쓰려면 USTRUCT여야 한다. */
USTRUCT()
struct FProjectileFreeList
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<APaintProjectile>> Projectiles;
};

/**
 * 페인트볼을 재사용한다. 한 판에서 초당 수백 개가 생기고 사라지는 유일한 액터라, 스폰과
 * 파괴 비용이 교전 중 프레임에 그대로 얹힌다.
 *
 * 페인트볼만 풀링하는 이유는 이것만 복제되지 않기 때문이다. 나머지 스폰 액터(버스트,
 * 페인트 레인, 아이템 투사체, 초코돔)는 전부 설정을 COND_InitialOnly로 나르므로, 재사용된
 * 액터는 클라이언트에 파라미터 없이 도착한다. 게다가 분당 몇 번 스폰될 뿐이라 이득도 없다.
 *
 * 풀이 비면 그냥 새로 스폰한다. 사격이 끊기는 일은 없고, 풀은 실사용량에 맞게 자란다.
 */
UCLASS()
class MINTCHOCO_API UProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UProjectilePoolSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * 공 하나를 날린다. 풀에 놀고 있는 것이 있으면 되살리고, 없으면 새로 스폰한다.
	 * 호출부는 어느 쪽인지 알 필요가 없다.
	 */
	APaintProjectile* Launch(
		TSubclassOf<APaintProjectile> Class,
		const FTransform& Where,
		APawn* Instigator,
		const UPaintballProfile* Profile,
		uint8 PaintId,
		int32 Seed,
		const FVector& Velocity,
		bool bCosmetic);

	/** 다 쓴 공을 돌려받는다. 파괴하지 않고 재워 둔다. */
	void Release(APaintProjectile* Projectile);

	/** 미리 만들어 재워 둔다. 워밍업이 로딩 화면 뒤에서 부른다. */
	void Prewarm(TSubclassOf<APaintProjectile> Class, int32 Count);

	/** 지금 날고 있는 공의 수와, 이 월드에서 기록한 최대치. */
	int32 GetLiveCount() const { return LiveCount; }
	int32 GetPeakLiveCount() const { return PeakLiveCount; }

	virtual void Deinitialize() override;

private:
	/** 풀에 없으면 새로 만든다. Init 전의, 트랜스폼만 잡힌 공을 돌려준다. */
	APaintProjectile* AcquireIdle(TSubclassOf<APaintProjectile> Class, const FTransform& Where, APawn* Instigator, bool& bOutFresh);

	/** 클래스마다 따로 논다. 무기마다 ProjectileClass가 다르기 때문이다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, FProjectileFreeList> FreeLists;

	int32 LiveCount = 0;
	int32 PeakLiveCount = 0;
};
