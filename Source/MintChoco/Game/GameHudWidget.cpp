#include "Game/GameHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

#include "MintChoco.h"
#include "Weapons/PaintCrosshairHostWidget.h"

// ---------------------------------------------------------------- FGameHudMath

int32 FGameHudMath::CeilSeconds(float Seconds)
{
	return FMath::CeilToInt(FMath::Max(Seconds, 0.0f));
}

EGameHudCenter FGameHudMath::CenterKind(EMatchPhase Phase, float MatchRemaining, float SinceStart, float StartTextDuration, float FinalCountdownSeconds)
{
	switch (Phase)
	{
	case EMatchPhase::WaitingForPlayers:
		return EGameHudCenter::Waiting;
	case EMatchPhase::Countdown:
		return EGameHudCenter::Countdown;
	case EMatchPhase::Playing:
		if (SinceStart >= 0.0f && SinceStart < StartTextDuration)
		{
			return EGameHudCenter::Start;
		}
		if (FinalCountdownSeconds > 0.0f && MatchRemaining > 0.0f && MatchRemaining <= FinalCountdownSeconds)
		{
			return EGameHudCenter::FinalCountdown;
		}
		return EGameHudCenter::None;
	case EMatchPhase::Ended:
	default:
		return EGameHudCenter::None;
	}
}

bool FGameHudMath::IsTimerWarning(EMatchPhase Phase, float MatchRemaining, float WarningSeconds)
{
	return Phase == EMatchPhase::Playing && MatchRemaining <= WarningSeconds;
}

// ---------------------------------------------------------------- UGameHudWidget

void UGameHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Txt_Timer)
	{
		TimerNormalColor = Txt_Timer->GetColorAndOpacity().GetSpecifiedColor();
	}
	if (Txt_Countdown)
	{
		CountdownNormalColor = CountdownColor;
		Txt_Countdown->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 블루프린트가 크로스헤어 자리를 놓지 않았으면 코드가 놓는다. 풀스크린이어야 위젯 중앙이 화면
	// 중앙이고 월드→위젯 투영이 그대로 로컬 좌표가 된다.
	if (!CrosshairHost && WidgetTree)
	{
		UCanvasPanel* const Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!Canvas)
		{
			UE_LOG(LogMintChoco, Warning, TEXT("%s: root is not a CanvasPanel, no crosshair was added."), *GetName());
			return;
		}
		CrosshairHost = WidgetTree->ConstructWidget<UPaintCrosshairHostWidget>(UPaintCrosshairHostWidget::StaticClass(), TEXT("CrosshairHost"));
		if (UCanvasPanelSlot* const CrosshairSlot = Canvas->AddChildToCanvas(CrosshairHost))
		{
			CrosshairSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CrosshairSlot->SetOffsets(FMargin(0.0f));
			// 중앙 카운트다운 글자 아래에 깔린다.
			CrosshairSlot->SetZOrder(-1);
		}
	}
}

void UGameHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* const World = GetWorld();
	const AGameGameState* const State = World ? World->GetGameState<AGameGameState>() : nullptr;
	if (!State)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	const EMatchPhase Phase = State->GetMatchPhase();

	// Playing에 들어온 순간을 잡는다. 처음 본 단계가 이미 Playing이면(늦게 합류) START!는 건너뛴다.
	if (!bSawPhase)
	{
		bSawPhase = true;
		LastPhase = Phase;
	}
	else if (Phase != LastPhase)
	{
		if (Phase == EMatchPhase::Playing)
		{
			PlayingEnteredAt = Now;
		}
		LastPhase = Phase;
	}

	const float Remaining = State->GetRemainingTime();
	UpdateTimer(*State, Remaining);
	UpdateCenter(*State, Remaining, Now);
}

void UGameHudWidget::UpdateTimer(const AGameGameState& State, float Remaining)
{
	if (!Txt_Timer)
	{
		return;
	}
	Txt_Timer->SetText(FText::AsNumber(FGameHudMath::CeilSeconds(Remaining)));
	const bool bWarning = FGameHudMath::IsTimerWarning(State.GetMatchPhase(), Remaining, TimerWarningSeconds);
	Txt_Timer->SetColorAndOpacity(FSlateColor(bWarning ? TimerWarningColor : TimerNormalColor));
}

void UGameHudWidget::UpdateCenter(const AGameGameState& State, float Remaining, double Now)
{
	if (!Txt_Countdown)
	{
		return;
	}

	const float SinceStart = PlayingEnteredAt >= 0.0 ? static_cast<float>(Now - PlayingEnteredAt) : -1.0f;
	const EGameHudCenter Kind = FGameHudMath::CenterKind(State.GetMatchPhase(), Remaining, SinceStart, StartTextDuration, FinalCountdownSeconds);

	FText Text;
	FLinearColor Color = CountdownNormalColor;
	switch (Kind)
	{
	case EGameHudCenter::Waiting:
		Text = WaitingText;
		break;
	case EGameHudCenter::Countdown:
		Text = FText::AsNumber(FGameHudMath::CeilSeconds(State.GetCountdownRemaining()));
		break;
	case EGameHudCenter::Start:
		Text = StartText;
		break;
	case EGameHudCenter::FinalCountdown:
		Text = FText::AsNumber(FGameHudMath::CeilSeconds(Remaining));
		Color = FinalCountdownColor;
		break;
	case EGameHudCenter::None:
	default:
		break;
	}

	if (Text.IsEmpty())
	{
		Txt_Countdown->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Txt_Countdown->SetText(Text);
	Txt_Countdown->SetColorAndOpacity(FSlateColor(Color));
	Txt_Countdown->SetVisibility(ESlateVisibility::HitTestInvisible);
}
