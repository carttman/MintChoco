#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Game/GameGameState.h"

#include "GameHudWidget.generated.h"

class UTextBlock;

/** 화면 중앙에 무엇을 띄울지. 순수 계산의 결과라 테스트가 월드 없이 검사한다. */
enum class EGameHudCenter : uint8
{
	None,
	/** 다른 플레이어를 기다리는 중. */
	Waiting,
	/** 경기 전 3, 2, 1. */
	Countdown,
	/** 경기가 시작된 직후의 "START!". */
	Start,
	/** 마지막 10초의 10, 9, ... */
	FinalCountdown,
};

/** HUD 표시의 순수 계산. */
struct MINTCHOCO_API FGameHudMath
{
	/** 남은 초를 올림한 정수. 0.01초도 1로 보인다: 화면의 0은 끝난 순간뿐이다. */
	static int32 CeilSeconds(float Seconds);

	/**
	 * 중앙 텍스트의 종류. SinceStart는 Playing에 들어온 뒤 지난 초(음수면 아직 모른다).
	 */
	static EGameHudCenter CenterKind(EMatchPhase Phase, float MatchRemaining, float SinceStart, float StartTextDuration, float FinalCountdownSeconds);

	/** 타이머를 경고색으로 보일지. 경기 중이고 남은 시간이 기준 이하일 때. */
	static bool IsTimerWarning(EMatchPhase Phase, float MatchRemaining, float WarningSeconds);
};

/**
 * 게임 HUD의 부모. 경기 단계에 맞춰 타이머 색과 중앙 카운트다운을 매 프레임 채운다.
 *
 * 위젯 블루프린트(WBP_GameHUD)가 이 클래스를 부모로 두고 Txt_Timer / Txt_Countdown이라는
 * 이름의 TextBlock을 가지면 자동으로 묶인다. 둘 다 선택이라 하나만 있어도 된다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UGameHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 남은 시간. 경기 전에는 한 판의 길이가 그대로 보인다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Timer;

	/** 화면 정중앙의 큰 글자: 대기 안내, 3·2·1, START!, 마지막 10초. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Countdown;

	/** 남은 시간이 이 값(초) 이하가 되면 타이머가 TimerWarningColor로 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD", meta = (ClampMin = "0", ForceUnits = "s"))
	float TimerWarningSeconds = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match HUD")
	FLinearColor TimerWarningColor = FLinearColor(1.0f, 0.15f, 0.15f, 1.0f);

	/** 남은 시간이 이 값(초) 이하가 되면 중앙에도 초가 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD", meta = (ClampMin = "0", ForceUnits = "s"))
	float FinalCountdownSeconds = 10.0f;

	/** 경기가 시작된 순간 중앙에 띄우는 글자와 그 길이(초). 0이면 띄우지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD")
	FText StartText = NSLOCTEXT("GameHud", "Start", "START!");

	UPROPERTY(EditDefaultsOnly, Category = "Match HUD", meta = (ClampMin = "0", ForceUnits = "s"))
	float StartTextDuration = 1.0f;

	/** 전원 준비를 기다리는 동안 중앙에 띄우는 글자. 비우면 아무것도 띄우지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD")
	FText WaitingText = NSLOCTEXT("GameHud", "Waiting", "플레이어를 기다리는 중...");

	/** 중앙 카운트다운의 색(대기 문구, 3·2·1, START!). */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD")
	FLinearColor CountdownColor = FLinearColor::White;

	/** 마지막 10초 중앙 숫자의 색. 흰색 반투명. */
	UPROPERTY(EditDefaultsOnly, Category = "Match HUD")
	FLinearColor FinalCountdownColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

private:
	/** Txt_Timer의 원래 색. 경고가 끝나면 되돌린다. */
	FLinearColor TimerNormalColor = FLinearColor::White;
	FLinearColor CountdownNormalColor = FLinearColor::White;

	/** Playing으로 들어온 순간의 월드 시각. START! 표시의 기준. 음수면 아직 들어오지 않았다. */
	double PlayingEnteredAt = -1.0;
	EMatchPhase LastPhase = EMatchPhase::WaitingForPlayers;
	bool bSawPhase = false;

	void UpdateTimer(const AGameGameState& State, float Remaining);
	void UpdateCenter(const AGameGameState& State, float Remaining, double Now);
};
