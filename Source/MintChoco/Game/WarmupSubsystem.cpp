#include "Game/WarmupSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "PipelineStateCache.h"
#include "UObject/UObjectGlobals.h"

#include "Game/GameGameState.h"
#include "Game/WarmupSettings.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "MintChoco.h"
#include "Screen/ScreenFadeSubsystem.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

namespace
{
	/** 홀드의 이유. 타임아웃 로그가 무엇이 안 끝났는지 이름으로 알려 준다. */
	const FName WarmupHoldReason(TEXT("Warmup"));
}

void UWarmupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 여기서는 월드를 건드리지 않는다. 스탠드얼론의 GameInstance는 버려질 DummyWorld를
	// 현재 월드로 둔 채 Init을 부른다(GameAudioSubsystem과 같은 이유).
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UWarmupSubsystem::HandlePostLoadMap);
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UWarmupSubsystem::HandleWorldInitializedActors);
}

void UWarmupSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	if (WorldInitHandle.IsValid())
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
		WorldInitHandle.Reset();
	}

	FinishWarmup(TEXT("종료"));

	Super::Deinitialize();
}

void UWarmupSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	BeginWarmup(LoadedWorld);
}

void UWarmupSubsystem::HandleWorldInitializedActors(const UWorld::FActorsInitializedParams& Params)
{
	UWorld* const World = Params.World;
	if (!IsValid(World) || World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}

	// 지금은 게임스테이트가 아직 없을 수 있어 게임 맵인지 판단할 수 없다. 월드가 실제로
	// 플레이를 시작하는 순간까지 미룬다.
	World->OnWorldBeginPlay.AddWeakLambda(this, [this, World]()
	{
		BeginWarmup(World);
	});
}

void UWarmupSubsystem::BeginWarmup(UWorld* World)
{
	const UWarmupSettings& Settings = UWarmupSettings::Get();
	if (!Settings.bEnabled || !IsValid(World))
	{
		return;
	}

	// 데디케이티드 서버에는 그릴 화면도, 데울 셰이더도 없다.
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 게임 맵만: 타이틀과 로비까지 붙잡으면 첫 화면만 늦어진다.
	if (Settings.bGameMapsOnly && !World->GetGameState<AGameGameState>())
	{
		return;
	}

	// 두 트리거(맵 로드, 월드 BeginPlay)가 같은 월드를 두 번 데우지 않게 한다.
	if (bWarming || WarmedWorld.Get() == World)
	{
		return;
	}

	bWarming = true;
	WarmingWorld = World;
	WarmupStartTime = FPlatformTime::Seconds();
	WarmedWorld = World;

	if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(World))
	{
		Fade->AddHold(this, WarmupHoldReason);
	}

	// 동기 작업은 지금 바로. 어차피 화면은 덮여 있다.
	if (Settings.bPreloadItems)
	{
		PreloadItems();
	}
	if (Settings.bPrewarmProjectilePool)
	{
		PrewarmProjectilePool(World);
	}

	// PSO는 렌더 스레드가 뒤에서 만든다. 큐가 빌 때까지만 기다린다.
	if (Settings.bWaitForPSOPrecache && PipelineStateCache::IsPSOPrecachingEnabled())
	{
		PSOTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UWarmupSubsystem::TickWaitForPSO), 0.0f);
		return;
	}

	FinishWarmup(TEXT("완료"));
}

bool UWarmupSubsystem::TickWaitForPSO(float DeltaTime)
{
	const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - WarmupStartTime);

	if (!WarmingWorld.IsValid())
	{
		FinishWarmup(TEXT("월드 사라짐"));
		return false;
	}

	if (Elapsed >= UWarmupSettings::Get().Timeout)
	{
		UE_LOG(LogMintChoco, Warning,
			TEXT("워밍업 상한 %.1f초를 넘겼습니다. PSO 요청 %u개가 남은 채로 화면을 엽니다."),
			UWarmupSettings::Get().Timeout, PipelineStateCache::NumActivePrecacheRequests());
		FinishWarmup(TEXT("상한 초과"));
		return false;
	}

	if (PipelineStateCache::NumActivePrecacheRequests() > 0)
	{
		return true;
	}

	FinishWarmup(TEXT("완료"));
	return false;
}

void UWarmupSubsystem::FinishWarmup(const TCHAR* Why)
{
	if (PSOTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PSOTickHandle);
		PSOTickHandle.Reset();
	}

	if (!bWarming)
	{
		return;
	}
	bWarming = false;

	if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(WarmingWorld.Get()))
	{
		Fade->RemoveHold(this, WarmupHoldReason);
	}
	WarmingWorld.Reset();

	UE_LOG(LogMintChoco, Verbose, TEXT("워밍업 %s: %.2f초."), Why, FPlatformTime::Seconds() - WarmupStartTime);
}

void UWarmupSubsystem::PreloadItems()
{
	if (PreloadedItems.Num() > 0)
	{
		return;
	}

	const UItemSettings& Settings = UItemSettings::Get();

	TArray<UItemProfile*> Loaded;
	Settings.LoadItems(Loaded);
	PreloadedItems.Append(Loaded);

	PreloadedPickupClass = Settings.LoadPickupClass();

	UE_LOG(LogMintChoco, Verbose, TEXT("워밍업: 아이템 프로필 %d개 미리 로드."), PreloadedItems.Num());
}

void UWarmupSubsystem::PrewarmProjectilePool(UWorld* World)
{
	UProjectilePoolSubsystem* const Pool = UProjectilePoolSubsystem::Get(World);
	if (!Pool)
	{
		return;
	}

	for (const FWarmupProjectileEntry& Entry : UWarmupSettings::Get().ProjectilePool)
	{
		if (Entry.Count <= 0 || Entry.ProjectileClass.IsNull())
		{
			continue;
		}

		if (UClass* const Class = Entry.ProjectileClass.LoadSynchronous())
		{
			Pool->Prewarm(Class, Entry.Count);
		}
	}
}
