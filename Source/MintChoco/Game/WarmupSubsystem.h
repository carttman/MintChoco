#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "WarmupSubsystem.generated.h"

class UItemProfile;

/**
 * 경기가 시작되기 전에 미리 데운다: 늦게 로드되는 에셋을 끌어오고, PSO 프리캐싱이 끝나기를
 * 기다리고, 페인트볼 풀을 채운다.
 *
 * 가림막(UScreenFadeSubsystem)에 홀드를 걸어 두는 것이 전부다. 홀드가 걸려 있으면 화면이
 * 열리지 않고, 화면이 덮여 있으면 AGamePlayerState가 준비를 보고하지 않고, 그러면
 * AGameGameMode가 카운트다운을 시작하지 않는다. 즉 홀드 하나로 화면과 경기 시작이 함께
 * 멈추므로 새 배선이 필요 없다. UPaintSubsystem이 아틀라스를 구울 때 쓰는 방식과 같다.
 *
 * GameInstance 수명이지만 실제 작업은 PostLoadMapWithWorld에서 시작한다. Initialize 시점의
 * 월드는 스탠드얼론에서 버려질 DummyWorld이기 때문이다.
 */
UCLASS()
class MINTCHOCO_API UWarmupSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 워밍업이 아직 돌고 있는지. */
	bool IsWarming() const { return bWarming; }

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/**
	 * PIE의 첫 월드는 에디터 월드를 복제해서 만들기 때문에 PostLoadMapWithWorld가 발생하지
	 * 않는다(가림막도 같은 이유로 PIE 첫 창에서는 안 보인다). 그래서 월드가 BeginPlay 할 때도
	 * 한 번 더 본다. 어느 쪽이 먼저 오든 워밍업은 한 월드에 한 번만 돈다.
	 */
	void HandleWorldInitializedActors(const UWorld::FActorsInitializedParams& Params);

	/** 맵이 올라올 때마다. 게임 맵이 아니거나 꺼져 있으면 아무것도 하지 않는다. */
	void BeginWarmup(UWorld* World);

	/** 홀드를 놓고 소요 시간을 남긴다. 두 번 불려도 안전하다. */
	void FinishWarmup(const TCHAR* Why);

	/** PSO 큐가 빌 때까지 폴링한다. 코어 티커라 로딩 중에도 돈다. */
	bool TickWaitForPSO(float DeltaTime);

	/** 동기 로드. 아이템 프로필과 픽업 클래스를 끌어오고 참조를 쥔다. */
	void PreloadItems();

	void PrewarmProjectilePool(UWorld* World);

	/**
	 * 미리 로드한 에셋을 붙잡아 둔다. 소프트 참조는 아무도 안 쥐면 다시 내려갈 수 있고,
	 * 그러면 경기 중 동기 로드가 그대로 돌아온다.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemProfile>> PreloadedItems;

	UPROPERTY(Transient)
	TObjectPtr<UClass> PreloadedPickupClass;

	/** 워밍업 대상 월드. 티커가 도는 동안 살아 있는지 확인한다. */
	TWeakObjectPtr<UWorld> WarmingWorld;

	/** 이미 데운 월드. 두 트리거가 같은 월드를 두 번 데우지 않게 한다. */
	TWeakObjectPtr<UWorld> WarmedWorld;

	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle WorldInitHandle;
	FTSTicker::FDelegateHandle PSOTickHandle;

	double WarmupStartTime = 0.0;
	bool bWarming = false;
};
