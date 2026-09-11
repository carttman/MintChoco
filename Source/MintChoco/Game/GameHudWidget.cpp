#include "Game/GameHudWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"

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
