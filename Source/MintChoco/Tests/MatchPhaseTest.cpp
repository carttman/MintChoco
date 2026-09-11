#include "Misc/AutomationTest.h"

#include "Game/GameGameMode.h"
#include "Game/GameGameState.h"
#include "Game/GameHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 경기 단계의 입력 규칙, 전원 준비 판정, HUD 중앙 표시와 타이머 경고의 순수 계산. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchPhaseTest,
	"MintChoco.Match.Phase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchPhaseTest::RunTest(const FString& Parameters)
{
	// 입력은 경기 중과 종료 후에만.
	TestFalse(TEXT("waiting locks input"), AGameGameState::AllowsPlayerInput(EMatchPhase::WaitingForPlayers));
	TestFalse(TEXT("countdown locks input"), AGameGameState::AllowsPlayerInput(EMatchPhase::Countdown));
	TestTrue(TEXT("playing allows input"), AGameGameState::AllowsPlayerInput(EMatchPhase::Playing));
	TestTrue(TEXT("ended allows input"), AGameGameState::AllowsPlayerInput(EMatchPhase::Ended));
	TestTrue(TEXT("no game state: always allowed"), AGameGameState::IsPlayerInputAllowed(nullptr));

	// 전원 준비: 한 명이라도 아니면 아니다. 아무도 없으면 시작하지 않는다.
	TestFalse(TEXT("nobody: not ready"), AGameGameMode::AreAllReady({}));
	TestFalse(TEXT("one of two: not ready"), AGameGameMode::AreAllReady({true, false}));
	TestTrue(TEXT("both: ready"), AGameGameMode::AreAllReady({true, true}));

	// 초 올림.
	TestEqual(TEXT("ceil 2.01 -> 3"), FGameHudMath::CeilSeconds(2.01f), 3);
	TestEqual(TEXT("ceil 3.0 -> 3"), FGameHudMath::CeilSeconds(3.0f), 3);
	TestEqual(TEXT("ceil negative -> 0"), FGameHudMath::CeilSeconds(-1.0f), 0);

	// 중앙 표시.
	const float StartDur = 1.0f;
	const float Final = 10.0f;
	TestEqual(TEXT("waiting"), FGameHudMath::CenterKind(EMatchPhase::WaitingForPlayers, 90.0f, -1.0f, StartDur, Final), EGameHudCenter::Waiting);
	TestEqual(TEXT("countdown"), FGameHudMath::CenterKind(EMatchPhase::Countdown, 90.0f, -1.0f, StartDur, Final), EGameHudCenter::Countdown);
	TestEqual(TEXT("start text right after start"), FGameHudMath::CenterKind(EMatchPhase::Playing, 90.0f, 0.5f, StartDur, Final), EGameHudCenter::Start);
	TestEqual(TEXT("nothing mid-match"), FGameHudMath::CenterKind(EMatchPhase::Playing, 60.0f, 30.0f, StartDur, Final), EGameHudCenter::None);
	TestEqual(TEXT("late joiner skips start text"), FGameHudMath::CenterKind(EMatchPhase::Playing, 60.0f, -1.0f, StartDur, Final), EGameHudCenter::None);
	TestEqual(TEXT("final ten seconds"), FGameHudMath::CenterKind(EMatchPhase::Playing, 9.5f, 80.5f, StartDur, Final), EGameHudCenter::FinalCountdown);
	TestEqual(TEXT("exactly ten counts"), FGameHudMath::CenterKind(EMatchPhase::Playing, 10.0f, 80.0f, StartDur, Final), EGameHudCenter::FinalCountdown);
	TestEqual(TEXT("ended shows nothing"), FGameHudMath::CenterKind(EMatchPhase::Ended, 0.0f, 90.0f, StartDur, Final), EGameHudCenter::None);

	// 타이머 경고: 경기 중 30초 이하.
	TestFalse(TEXT("no warning before start"), FGameHudMath::IsTimerWarning(EMatchPhase::Countdown, 90.0f, 30.0f));
	TestFalse(TEXT("no warning at 31"), FGameHudMath::IsTimerWarning(EMatchPhase::Playing, 31.0f, 30.0f));
	TestTrue(TEXT("warning at 30"), FGameHudMath::IsTimerWarning(EMatchPhase::Playing, 30.0f, 30.0f));
	TestTrue(TEXT("warning at 5"), FGameHudMath::IsTimerWarning(EMatchPhase::Playing, 5.0f, 30.0f));

	return true;
}

#endif
