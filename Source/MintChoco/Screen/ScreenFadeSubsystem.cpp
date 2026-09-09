#include "Screen/ScreenFadeSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"

#include "Game/GameGameState.h"
#include "MintChoco.h"
#include "Screen/ScreenFadeRelay.h"
#include "Screen/ScreenFadeSettings.h"

namespace
{
	constexpr int32 CoverZOrder = 10000;
}

// ---------------------------------------------------------------- FScreenFadeState

void FScreenFadeState::StartFadeOut()
{
	if (Phase == EScreenFadePhase::Clear || Phase == EScreenFadePhase::FadingIn)
	{
		// 밝아지던 중이면 지금 밝기에서 그대로 어두워진다. 불투명도는 속도 기반이라 이어진다.
		Phase = EScreenFadePhase::FadingOut;
		Elapsed = 0.0f;
	}
}

void FScreenFadeState::Cover()
{
	Phase = EScreenFadePhase::Covered;
	Opacity = 1.0f;
	Elapsed = 0.0f;
}

void FScreenFadeState::StartWaiting()
{
	Phase = EScreenFadePhase::WaitingForReady;
	Opacity = 1.0f;
	Elapsed = 0.0f;
}

void FScreenFadeState::StartFadeIn()
{
	if (Phase != EScreenFadePhase::Clear)
	{
		Phase = EScreenFadePhase::FadingIn;
		Elapsed = 0.0f;
	}
}

bool FScreenFadeState::Tick(float DeltaTime, float FadeDuration, bool bReady, float MaxWait)
{
	const float Step = FadeDuration > 0.0f ? DeltaTime / FadeDuration : 1.0f;
	Elapsed += DeltaTime;

	switch (Phase)
	{
	case EScreenFadePhase::FadingOut:
		Opacity = FMath::Min(1.0f, Opacity + Step);
		if (Opacity >= 1.0f)
		{
			Cover();
			return true;
		}
		return false;

	case EScreenFadePhase::WaitingForReady:
		if (bReady || (MaxWait > 0.0f && Elapsed >= MaxWait))
		{
			StartFadeIn();
			return true;
		}
		return false;

	case EScreenFadePhase::FadingIn:
		Opacity = FMath::Max(0.0f, Opacity - Step);
		if (Opacity <= 0.0f)
		{
			Phase = EScreenFadePhase::Clear;
			Elapsed = 0.0f;
			return true;
		}
		return false;

	case EScreenFadePhase::Covered:
	case EScreenFadePhase::Clear:
	default:
		return false;
	}
}

// ---------------------------------------------------------------- UScreenFadeSubsystem

UScreenFadeSubsystem* UScreenFadeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UScreenFadeSubsystem>() : nullptr;
}

void UScreenFadeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UScreenFadeSubsystem::HandlePreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UScreenFadeSubsystem::HandlePostLoadMap);

	// 월드 틱이 아니라 코어 티커다. 로드 중에는 월드가 서지만 페이드는 실제 시간으로 흘러야 한다.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UScreenFadeSubsystem::Tick), 0.0f);
}

void UScreenFadeSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	HideCover();
	FallbackCover.Reset();
	CoverWidget = nullptr;

	Super::Deinitialize();
}

void UScreenFadeSubsystem::TravelWithFade(const FString& URL, bool bServerTravel)
{
	if (bServerTravel)
	{
		ServerTravelWithFade(URL);
	}
	else
	{
		ClientTravelWithFade(URL);
	}
}

void UScreenFadeSubsystem::ServerTravelWithFade(const FString& URL)
{
	UWorld* const World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("ServerTravelWithFade(%s): 서버가 아닌 곳에서 불렸다. 무시한다."), *URL);
		return;
	}
	if (bTravelQueued)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("ServerTravelWithFade(%s): 이미 %s로 떠나는 중이라 무시한다."), *URL, *QueuedURL);
		return;
	}

	QueuedURL = URL;
	bQueuedServerTravel = true;
	bTravelQueued = true;
	TravelHoldElapsed = 0.0f;

	// 릴레이의 멀티캐스트는 서버 자신에서도 돌므로 여기서 따로 FadeOut을 부르지 않는다.
	if (AScreenFadeRelay* const Relay = World->SpawnActor<AScreenFadeRelay>())
	{
		Relay->MulticastFadeOut(UScreenFadeSettings::Get().FadeDuration);
	}
	else
	{
		FadeOut();
	}
}

void UScreenFadeSubsystem::ClientTravelWithFade(const FString& URL)
{
	if (bTravelQueued)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("ClientTravelWithFade(%s): 이미 %s로 떠나는 중이라 무시한다."), *URL, *QueuedURL);
		return;
	}

	QueuedURL = URL;
	bQueuedServerTravel = false;
	bTravelQueued = true;
	TravelHoldElapsed = 0.0f;
	FadeOut();
}

void UScreenFadeSubsystem::FadeOut(float Duration)
{
	State.StartFadeOut();
	EnsureCoverShown();
	ApplyOpacity();
}

void UScreenFadeSubsystem::AddHold(const UObject* Owner, FName Reason)
{
	for (FHold& Hold : Holds)
	{
		if (Hold.Owner == Owner && Hold.Reason == Reason)
		{
			++Hold.Count;
			return;
		}
	}
	FHold& Hold = Holds.AddDefaulted_GetRef();
	Hold.Owner = Owner;
	Hold.Reason = Reason;
	Hold.Count = 1;
}

void UScreenFadeSubsystem::RemoveHold(const UObject* Owner, FName Reason)
{
	for (int32 Index = 0; Index < Holds.Num(); ++Index)
	{
		FHold& Hold = Holds[Index];
		if (Hold.Owner == Owner && Hold.Reason == Reason)
		{
			if (--Hold.Count <= 0)
			{
				Holds.RemoveAtSwap(Index);
			}
			return;
		}
	}
}

bool UScreenFadeSubsystem::HasHolds() const
{
	for (const FHold& Hold : Holds)
	{
		// 소유자가 사라진 홀드는 지난 월드의 것이다. 세지 않는다.
		if (Hold.Owner.IsValid() && Hold.Count > 0)
		{
			return true;
		}
	}
	return false;
}

bool UScreenFadeSubsystem::IsWorldReady(const UWorld* World) const
{
	if (!World || World->IsInSeamlessTravel() || World->bIsTearingDown)
	{
		return false;
	}
	if (!World->AreAlwaysLoadedLevelsLoaded() || World->IsVisibilityRequestPending())
	{
		return false;
	}

	const APlayerController* const PlayerController = World->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->PlayerState)
	{
		return false;
	}

	// 게임 맵은 내 캐릭터가 서 있어야 열린다. 메뉴 맵에는 폰이 없을 수 있으므로 묻지 않는다.
	if (World->GetGameState<AGameGameState>() && !PlayerController->GetPawn())
	{
		return false;
	}

	return !HasHolds();
}

bool UScreenFadeSubsystem::Tick(float DeltaTime)
{
	const UScreenFadeSettings& Settings = UScreenFadeSettings::Get();
	UWorld* const World = GetWorld();

	const EScreenFadePhase Before = State.Phase;
	const float WaitedBefore = State.Elapsed;
	const bool bReady = Before == EScreenFadePhase::WaitingForReady && IsWorldReady(World);
	State.Tick(DeltaTime, Settings.FadeDuration, bReady, Settings.MaxReadyWait);

	if (Before == EScreenFadePhase::WaitingForReady && State.Phase == EScreenFadePhase::FadingIn && !bReady && !bWarnedTimeout)
	{
		bWarnedTimeout = true;
		TArray<FString> Reasons;
		for (const FHold& Hold : Holds)
		{
			if (Hold.Owner.IsValid())
			{
				Reasons.Add(FString::Printf(TEXT("%s:%s"), *GetNameSafe(Hold.Owner.Get()), *Hold.Reason.ToString()));
			}
		}
		UE_LOG(LogMintChoco, Warning, TEXT("가림막: %.1f초가 지나도 맵이 준비되지 않아 그냥 연다. 남은 홀드: %s"),
			Settings.MaxReadyWait, Reasons.IsEmpty() ? TEXT("(없음; 컨트롤러/폰 대기)") : *FString::Join(Reasons, TEXT(", ")));
	}

	if (Before == EScreenFadePhase::WaitingForReady && State.Phase == EScreenFadePhase::FadingIn && bReady)
	{
		UE_LOG(LogMintChoco, Log, TEXT("가림막: %s 준비됨(%.2f초 대기), 연다."), *GetNameSafe(World), WaitedBefore + DeltaTime);
	}

	if (bTravelQueued && State.IsDark())
	{
		TravelHoldElapsed += DeltaTime;
		if (TravelHoldElapsed >= Settings.TravelHold)
		{
			PerformQueuedTravel();
		}
	}

	if (State.IsClear())
	{
		HideCover();
	}
	else
	{
		EnsureCoverShown();
		ApplyOpacity();
	}
	return true;
}

void UScreenFadeSubsystem::PerformQueuedTravel()
{
	bTravelQueued = false;
	const FString URL = QueuedURL;
	QueuedURL.Empty();

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bQueuedServerTravel)
	{
		UE_LOG(LogMintChoco, Log, TEXT("가림막: 서버 트래블 %s"), *URL);
		World->ServerTravel(URL);
	}
	else if (APlayerController* const PlayerController = World->GetFirstPlayerController())
	{
		UE_LOG(LogMintChoco, Log, TEXT("가림막: 클라이언트 트래블 %s"), *URL);
		PlayerController->ClientTravel(URL, TRAVEL_Absolute);
	}
}

void UScreenFadeSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	// 다른 PIE 인스턴스의 로드는 우리 화면과 무관하다.
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	// 로드가 시작된다. 어둡지 않았다면(페이드가 덜 끝났거나 우리가 시작한 트래블이 아니면) 지금 어둡게 한다.
	UE_LOG(LogMintChoco, Log, TEXT("가림막: %s 로드 시작, 어둡게 고정."), *MapName);
	State.Cover();
	bWarnedTimeout = false;
	bFallbackShown = false;
	EnsureCoverShown();
	ApplyOpacity();
}

void UScreenFadeSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 새 뷰포트는 비어 있다. 첫 프레임이 그려지기 전에 가림막을 다시 얹어야 번쩍이지 않는다.
	UE_LOG(LogMintChoco, Log, TEXT("가림막: %s 로드 완료, 준비를 기다린다."), *GetNameSafe(LoadedWorld));
	bFallbackShown = false;
	bTravelQueued = false;
	QueuedURL.Empty();
	State.StartWaiting();
	EnsureCoverShown();
	ApplyOpacity();
}

void UScreenFadeSubsystem::EnsureCoverShown()
{
	UGameInstance* const GameInstance = GetGameInstance();
	UGameViewportClient* const Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		return;
	}

	if (!CoverWidget)
	{
		const UScreenFadeSettings& Settings = UScreenFadeSettings::Get();
		UClass* const WidgetClass = Settings.LoadingWidgetClass.IsNull() ? nullptr : Settings.LoadingWidgetClass.LoadSynchronous();
		if (WidgetClass)
		{
			// 소유자가 GameInstance라 맵이 바뀌어도 같은 위젯이 살아남는다.
			CoverWidget = CreateWidget<UUserWidget>(GameInstance, WidgetClass);
			if (CoverWidget)
			{
				CoverWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}

	if (CoverWidget)
	{
		if (!CoverWidget->IsInViewport())
		{
			CoverWidget->AddToViewport(CoverZOrder);
		}
		return;
	}

	if (!FallbackCover.IsValid())
	{
		FallbackCover = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::Black)
			.Visibility(EVisibility::HitTestInvisible);
	}
	if (!bFallbackShown)
	{
		Viewport->AddViewportWidgetContent(FallbackCover.ToSharedRef(), CoverZOrder);
		bFallbackShown = true;
	}
}

void UScreenFadeSubsystem::HideCover()
{
	if (CoverWidget && CoverWidget->IsInViewport())
	{
		CoverWidget->RemoveFromParent();
	}

	if (bFallbackShown)
	{
		UGameInstance* const GameInstance = GetGameInstance();
		UGameViewportClient* const Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
		if (Viewport && FallbackCover.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(FallbackCover.ToSharedRef());
		}
		bFallbackShown = false;
	}
}

void UScreenFadeSubsystem::ApplyOpacity()
{
	if (CoverWidget)
	{
		CoverWidget->SetRenderOpacity(State.Opacity);
	}
	if (FallbackCover.IsValid())
	{
		FallbackCover->SetRenderOpacity(State.Opacity);
	}
}
