#include "Game/MatchResultSubsystem.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "Game/GameGameState.h"
#include "Game/GameHudWidget.h"
#include "Game/MatchResultFrameWidget.h"
#include "Game/MatchResultSettings.h"
#include "Game/MatchResultStage.h"
#include "Game/PaintBarWidget.h"
#include "Game/Unit.h"
#include "Paint/PaintableComponent.h"
#include "Screen/ScreenFadeSettings.h"
#include "Screen/ScreenFadeSubsystem.h"
#include "MintChoco.h"

namespace MatchResultSubsystem
{
	/** 길이가 0인 단계도 한 프레임 뒤에는 넘어가야 한다. 타이머는 0 으로 걸면 울리지 않는다. */
	constexpr float MinPhaseSeconds = 1.0e-3f;

	/** 테두리는 바 아래에 깐다. 그림이 겹치지는 않지만 바가 가려질 여지를 아예 없앤다. */
	constexpr int32 FrameZOrder = 0;
	constexpr int32 BarZOrder = 1;

	TSubclassOf<UPaintBarWidget> ResolveBarClass()
	{
		const UMatchResultSettings& Settings = UMatchResultSettings::Get();
		UClass* const Configured = Settings.BarWidgetClass.LoadSynchronous();
		return Configured ? Configured : UPaintBarWidget::StaticClass();
	}

	FAutoConsoleCommandWithWorldAndArgs GResultPreviewCommand(
		TEXT("mc.Result.Preview"),
		TEXT("경기 결과 연출을 지어낸 값으로 돌린다. mc.Result.Preview <민트 %> <초코 %> [이긴 팀 0|1|-1]. 인자 없이 부르면 멈춘다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMatchResultSubsystem* const Result = World ? World->GetSubsystem<UMatchResultSubsystem>() : nullptr;
			if (!Result)
			{
				UE_LOG(LogMintChoco, Warning, TEXT("mc.Result.Preview: 게임 월드(PIE 포함)에서만 쓸 수 있다."));
				return;
			}

			if (Args.Num() < 2)
			{
				Result->Abort();
				UE_LOG(LogMintChoco, Log, TEXT("mc.Result.Preview: 연출을 멈추고 경기 화면으로 되돌렸다."));
				return;
			}

			const float Left = FCString::Atof(*Args[0]) / 100.0f;
			const float Right = FCString::Atof(*Args[1]) / 100.0f;
			const int32 Winner = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : Teams::None;
			Result->BeginPreview(Left, Right, Winner);
		}));
}

APlayerController* UMatchResultSubsystem::FindLocalController() const
{
	UWorld* const World = GetWorld();
	return World && GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
}

UMatchResultSubsystem* UMatchResultSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMatchResultSubsystem>() : nullptr;
}

bool UMatchResultSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMatchResultSubsystem::Deinitialize()
{
	// 월드가 통째로 사라지는 길이라 되돌릴 것이 없다. 위젯만 걷고 나간다.
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}
	if (Bar)
	{
		Bar->RemoveFromParent();
		Bar = nullptr;
	}
	if (Frame)
	{
		Frame->RemoveFromParent();
		Frame = nullptr;
	}
	Stage = nullptr;
	bSpawnedStage = false;
	Phase = EMatchResultPhase::Idle;

	Super::Deinitialize();
}

float UMatchResultSubsystem::GetTotalSeconds()
{
	return MakeTimeline().GetTotalSeconds();
}

FMatchResultTimeline UMatchResultSubsystem::MakeTimeline()
{
	const UMatchResultSettings& Settings = UMatchResultSettings::Get();

	FMatchResultTimeline Result;
	Result.FreezeSeconds = Settings.FreezeSeconds;
	// 가림막의 길이는 가림막이 정한다. 여기서 따로 들고 있으면 둘이 어긋나 검은 화면이 깜빡인다.
	Result.FadeSeconds = UScreenFadeSettings::Get().FadeDuration;
	Result.BarEmptySeconds = Settings.BarEmptySeconds;
	Result.BarTeaserSeconds = Settings.BarTeaserSeconds;
	Result.BarRealSeconds = Settings.BarRealSeconds;
	Result.BarFinishSeconds = Settings.BarFinishSeconds;
	Result.CharacterSeconds = Settings.CharacterSeconds;
	Result.HoldSeconds = Settings.HoldSeconds;
	return Result;
}

void UMatchResultSubsystem::BeginSequence()
{
	const UWorld* const World = GetWorld();
	const AGameGameState* const State = World ? World->GetGameState<AGameGameState>() : nullptr;
	if (!State)
	{
		return;
	}

	FMatchResultInput Input;
	Input.WinningTeam = State->GetWinningTeam();
	Start(Input, /*bInPreview=*/false);
}

void UMatchResultSubsystem::BeginPreview(float LeftCoverage, float RightCoverage, int32 WinningTeam)
{
	// 다시 부르면 처음부터. 미리보기는 몇 번이고 돌려 보는 것이 목적이다.
	Abort();

	FMatchResultInput Input;
	Input.LeftCoverage = FMath::Clamp(LeftCoverage, 0.0f, 1.0f);
	Input.RightCoverage = FMath::Clamp(RightCoverage, 0.0f, 1.0f);
	Input.WinningTeam = WinningTeam;
	Start(Input, /*bInPreview=*/true);
}

void UMatchResultSubsystem::Start(const FMatchResultInput& InResult, bool bInPreview)
{
	UWorld* const World = GetWorld();
	if (IsRunning() || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!FindLocalController())
	{
		// 볼 사람이 없는 머신이다.
		return;
	}

	bPreview = bInPreview;
	Result = InResult;
	Result.TeaserCoverage = UMatchResultSettings::Get().TeaserCoverage;

	// 어느 팀이 어느 자리인지는 바가 정한다. 캐릭터도 같은 값을 따라 좌우가 늘 게이지와 맞는다.
	const UPaintBarWidget* const BarDefaults = GetDefault<UPaintBarWidget>(MatchResultSubsystem::ResolveBarClass());
	Result.LeftTeam = BarDefaults->GetLeftPaintId();
	Result.RightTeam = BarDefaults->GetRightPaintId();

	Timeline = MakeTimeline();
	EnterPhase(EMatchResultPhase::Frozen);
}

void UMatchResultSubsystem::Abort()
{
	UWorld* const World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
	}

	if (Phase != EMatchResultPhase::Idle)
	{
		ReleaseView();
	}

	if (Bar)
	{
		Bar->RemoveFromParent();
		Bar = nullptr;
	}
	if (Frame)
	{
		Frame->RemoveFromParent();
		Frame = nullptr;
	}
	if (Stage)
	{
		Stage->Dismiss();
		if (bSpawnedStage)
		{
			Stage->Destroy();
		}
		Stage = nullptr;
	}
	bSpawnedStage = false;
	Phase = EMatchResultPhase::Idle;
}

void UMatchResultSubsystem::EnterPhase(EMatchResultPhase NewPhase)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	Phase = NewPhase;

	switch (NewPhase)
	{
	case EMatchResultPhase::Frozen:
		// 입력은 단계가 Ended 로 바뀌면서 이미 잠겼다. 여기서는 기다리기만 한다.
		break;

	case EMatchResultPhase::FadingOut:
		if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(World))
		{
			Fade->FadeOut(Timeline.GetPhaseSeconds(EMatchResultPhase::FadingOut));
		}
		break;

	case EMatchResultPhase::Reveal:
		TakeOverView();
		if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(World))
		{
			Fade->FadeIn(Timeline.FadeSeconds);
		}
		break;

	case EMatchResultPhase::BarReal:
		if (!bPreview)
		{
			// 마지막 복제가 도착할 시간을 벌고 여기서 읽는다.
			if (const AGameGameState* const State = World->GetGameState<AGameGameState>())
			{
				const FPaintCoverage& Coverage = State->GetWorldCoverage();
				Result.LeftCoverage = Coverage.GetFraction(static_cast<uint8>(FMath::Max(Result.LeftTeam, 0)));
				Result.RightCoverage = Coverage.GetFraction(static_cast<uint8>(FMath::Max(Result.RightTeam, 0)));
			}
		}
		break;

	case EMatchResultPhase::Characters:
		if (Stage)
		{
			Stage->PlayFinish(Result.WinningTeam);
		}
		break;

	case EMatchResultPhase::Hold:
		// 다가오기가 끝나고 승리 모션이 도는 순간이다. 테두리도 같이 밝아진다.
		if (Frame)
		{
			Frame->FadeIn(UMatchResultSettings::Get().FrameFadeSeconds);
		}
		break;

	default:
		break;
	}

	if (Phase != NewPhase)
	{
		// 이 단계의 일이 연출을 접었다(무대를 못 만든 경우). 다음 단계를 예약하면 되살아난다.
		return;
	}

	PushBar();

	const EMatchResultPhase Next = FMatchResultTimeline::GetNextPhase(NewPhase);
	if (Next == EMatchResultPhase::Idle)
	{
		// 마지막 단계. 그림은 그대로 두고, 화면을 넘기는 것은 서버의 로비 복귀 타이머다.
		return;
	}

	const float Seconds = FMath::Max(Timeline.GetPhaseSeconds(NewPhase), MatchResultSubsystem::MinPhaseSeconds);
	World->GetTimerManager().SetTimer(PhaseTimer,
		FTimerDelegate::CreateUObject(this, &UMatchResultSubsystem::EnterPhase, Next), Seconds, /*bLoop=*/false);
}

void UMatchResultSubsystem::TakeOverView()
{
	UWorld* const World = GetWorld();
	APlayerController* const Controller = FindLocalController();
	if (!Controller)
	{
		return;
	}

	Stage = FindOrSpawnStage();
	if (!Stage)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("결과 연출: 무대를 만들지 못해 연출을 접는다."));
		Abort();
		return;
	}

	const AGameGameState* const State = World->GetGameState<AGameGameState>();
	const auto MakeCast = [State](int32 Team)
	{
		FMatchResultSlotCast Cast;
		Cast.Team = Team;
		Cast.UnitData = State ? State->FindTeamUnitData(Team) : nullptr;
		return Cast;
	};
	Stage->Prepare(MakeCast(Result.LeftTeam), MakeCast(Result.RightTeam));

	SetMatchVisualsHidden(true);
	CreateFrame();
	CreateBar();

	// 화면이 완전히 덮여 있는 동안이라 블렌드할 것이 없다.
	Controller->SetViewTarget(Stage);
}

void UMatchResultSubsystem::ReleaseView()
{
	APlayerController* const Controller = FindLocalController();

	SetMatchVisualsHidden(false);

	if (Controller)
	{
		AActor* const Own = Controller->GetPawn() ? static_cast<AActor*>(Controller->GetPawn()) : static_cast<AActor*>(Controller);
		Controller->SetViewTarget(Own);
	}
}

AMatchResultStage* UMatchResultSubsystem::FindOrSpawnStage()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	bSpawnedStage = false;
	for (TActorIterator<AMatchResultStage> It(World); It; ++It)
	{
		return *It;
	}

	UClass* const Configured = UMatchResultSettings::Get().FallbackStageClass.LoadSynchronous();
	UClass* const StageClass = Configured ? Configured : AMatchResultStage::StaticClass();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMatchResultStage* const Spawned = World->SpawnActor<AMatchResultStage>(StageClass, FTransform::Identity, Params);
	if (!Spawned)
	{
		return nullptr;
	}

	bSpawnedStage = true;
	Spawned->FrameBounds(MeasurePaintBounds());
	return Spawned;
}

FBox UMatchResultSubsystem::MeasurePaintBounds() const
{
	FBox Bounds(ForceInit);
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return Bounds;
	}

	// 칠할 수 있는 것 전부가 곧 "맵 전체"다. 바닥 밖의 장식은 프레임에 들어오지 않아도 된다.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UPaintableComponent>())
		{
			Bounds += It->GetComponentsBoundingBox(/*bNonColliding=*/true);
		}
	}
	return Bounds;
}

void UMatchResultSubsystem::CreateBar()
{
	UWorld* const World = GetWorld();
	APlayerController* const Controller = FindLocalController();
	if (!Controller)
	{
		return;
	}

	const UMatchResultSettings& Settings = UMatchResultSettings::Get();
	Bar = CreateWidget<UPaintBarWidget>(Controller, MatchResultSubsystem::ResolveBarClass());
	if (!Bar)
	{
		return;
	}

	Bar->SetBarSize(Settings.BarSize);
	Bar->SetFillSmoothingSeconds(Settings.BarFillSmoothingSeconds);

	// 미리보기 값을 넣는 순간 바는 GameState 에서 떨어져 나가므로, 격돌 문턱과 판정선은 여기서 넘겨준다.
	if (const AGameGameState* const State = World->GetGameState<AGameGameState>())
	{
		Bar->SetMatchRules(State->GetClashCoverage(), State->GetKnockoutLine());
	}

	Bar->AddToViewport();

	// 뷰포트 위젯은 기본이 전체 화면이다. 화면 아래 중앙에 제 크기로 고정한다.
	if (UGameViewportSubsystem* const Viewport = UGameViewportSubsystem::Get())
	{
		FGameViewportWidgetSlot Slot = Viewport->GetWidgetSlot(Bar);
		Slot.Anchors = FAnchors(0.5f, 1.0f);
		Slot.Alignment = FVector2D(0.5f, 1.0f);
		Slot.Offsets = FMargin(0.0f, -Settings.BarBottomOffset,
			static_cast<float>(Settings.BarSize.X), static_cast<float>(Settings.BarSize.Y));
		Slot.ZOrder = MatchResultSubsystem::BarZOrder;
		Viewport->SetWidgetSlot(Bar, Slot);
	}
}

void UMatchResultSubsystem::CreateFrame()
{
	APlayerController* const Controller = FindLocalController();
	const UMatchResultSettings& Settings = UMatchResultSettings::Get();
	UTexture2D* const Texture = Settings.FrameTexture.LoadSynchronous();
	if (!Controller || !Texture)
	{
		return;
	}

	Frame = CreateWidget<UMatchResultFrameWidget>(Controller, UMatchResultFrameWidget::StaticClass());
	if (!Frame)
	{
		return;
	}

	Frame->SetFrameTexture(Texture);
	Frame->AddToViewport(MatchResultSubsystem::FrameZOrder);
}

void UMatchResultSubsystem::PushBar()
{
	if (!Bar)
	{
		return;
	}

	const FMatchResultBarState State = FMatchResultMath::MakeBarState(Phase, Result);

	FPaintBarPreview Preview;
	Preview.bEnabled = true;
	Preview.bLoopDemo = false;
	Preview.LeftCoverage = State.LeftCoverage;
	Preview.RightCoverage = State.RightCoverage;
	Preview.ForcedKnockoutTeam = State.ForcedKnockoutTeam;
	Bar->SetCoverageOverride(Preview);
}

void UMatchResultSubsystem::SetMatchVisualsHidden(bool bHidden)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이 머신의 화면에서만 치운다. 숨김은 복제되지 않으므로 다른 클라이언트의 연출과 간섭하지 않는다.
	for (TActorIterator<AUnit> It(World); It; ++It)
	{
		It->SetActorHiddenInGame(bHidden);
	}

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UGameHudWidget::StaticClass(), /*TopLevelOnly=*/true);
	for (UUserWidget* const Widget : Widgets)
	{
		if (UGameHudWidget* const Hud = Cast<UGameHudWidget>(Widget))
		{
			Hud->SetHudVisible(!bHidden);
		}
	}
}
