#include "Game/GameHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "Game/GamePlayerState.h"
#include "MintChoco.h"
#include "Weapons/PaintCrosshairHostWidget.h"
#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"

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

FGameplayTag FGameHudMath::CountdownTickTag(EGameHudCenter Kind, int32 Number)
{
	// 경기 시작 카운트다운만 숫자별 소리를 쓴다. 경기 끝 10초가 3에 닿아도 여기로 들어오지
	// 않으므로 시작을 알리는 목소리가 그쪽으로 새지 않는다.
	if (Kind == EGameHudCenter::Countdown)
	{
		switch (Number)
		{
		case 3: return AudioTags::Audio_Match_Countdown_3;
		case 2: return AudioTags::Audio_Match_Countdown_2;
		case 1: return AudioTags::Audio_Match_Countdown_1;
		default: break;
		}
	}

	// 전용 소리를 정하지 않은 숫자(카운트다운을 3초보다 길게 둔 경우의 5·4 등)와 막판 초읽기.
	return AudioTags::Audio_Match_CountdownTick;
}

UTexture2D* FGameHudMath::CharacterImageFor(int32 Team, UTexture2D* Mint, UTexture2D* Choco)
{
	if (Team == Teams::Mint)
	{
		return Mint;
	}
	if (Team == Teams::Choco)
	{
		return Choco;
	}
	return nullptr;
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
	UpdateCharacterImage();
}

void UGameHudWidget::UpdateCharacterImage()
{
	if (!Img_Character)
	{
		return;
	}

	const AGamePlayerState* const PlayerState = GetOwningPlayerState<AGamePlayerState>();
	const int32 Team = PlayerState ? PlayerState->GetTeam() : Teams::None;
	if (bCharacterImageSet && Team == LastCharacterTeam)
	{
		return;
	}

	UTexture2D* const Image = FGameHudMath::CharacterImageFor(Team, MintCharacterImage, ChocoCharacterImage);
	if (!Image)
	{
		// 아직 팀이 없다(관전, 고르기 전, PlayerState가 오기 전). 블루프린트가 넣어 둔 그림을
		// 그대로 두고 다음 틱에 다시 본다 — 여기서 비우면 팀이 정해질 때까지 HUD에 구멍이 난다.
		return;
	}

	// 크기는 블루프린트가 정한 대로 둔다. 텍스처 크기에 맞추면 배치가 흐트러진다.
	Img_Character->SetBrushFromTexture(Image, /*bMatchSize=*/false);
	LastCharacterTeam = Team;
	bCharacterImageSet = true;
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

	// 경고선을 넘는 순간 한 번: 경고음, 그리고 뱅크에 막판 곡이 있으면 그쪽으로 갈아탄다.
	if (bWarning && !bWasWarning)
	{
		UGameAudioSubsystem::Play2D(this, AudioTags::Audio_Match_TimerWarning);
		if (UGameAudioSubsystem* const Audio = UGameAudioSubsystem::Get(this))
		{
			if (Audio->HasEvent(AudioTags::Audio_Music_FinalRush))
			{
				Audio->PlayMusic(AudioTags::Audio_Music_FinalRush);
			}
		}
	}
	bWasWarning = bWarning;
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
	int32 Number = 0;
	switch (Kind)
	{
	case EGameHudCenter::Waiting:
		Text = WaitingText;
		break;
	case EGameHudCenter::Countdown:
		Number = FGameHudMath::CeilSeconds(State.GetCountdownRemaining());
		Text = FText::AsNumber(Number);
		break;
	case EGameHudCenter::Start:
		Text = StartText;
		break;
	case EGameHudCenter::FinalCountdown:
		Number = FGameHudMath::CeilSeconds(Remaining);
		Text = FText::AsNumber(Number);
		Color = FinalCountdownColor;
		break;
	case EGameHudCenter::None:
	default:
		break;
	}

	// 초읽기는 숫자가 바뀌는 프레임에 한 번. 어떤 소리를 낼지는 숫자와 상황이 정한다.
	if (Number > 0 && Number != LastCountdownNumber)
	{
		UGameAudioSubsystem::Play2D(this, FGameHudMath::CountdownTickTag(Kind, Number));
	}
	LastCountdownNumber = Number;

	if (Text.IsEmpty())
	{
		Txt_Countdown->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Txt_Countdown->SetText(Text);
	Txt_Countdown->SetColorAndOpacity(FSlateColor(Color));
	Txt_Countdown->SetVisibility(ESlateVisibility::HitTestInvisible);
}
