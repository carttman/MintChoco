#include "Misc/AutomationTest.h"

#include "Game/GameGameMode.h"
#include "Game/GameGameState.h"
#include "Game/GameHudWidget.h"
#include "Game/TeamTypes.h"

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

/**
 * 결과 화면 문구. 팀 이름을 코드와 위젯 양쪽에 적으면 한쪽만 고쳐져 어긋나므로, 문구가
 * Teams::GetDisplayName에서 온다는 것까지 여기서 못 박는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultTextTest,
	"MintChoco.Match.ResultText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultTextTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("민트가 이기면 민트 팀 승리"),
		AGameGameState::MakeMatchResultText(/*bEnded=*/true, Teams::Mint).ToString(), TEXT("민트 팀 승리"));
	TestEqual(TEXT("초코가 이기면 초코 팀 승리"),
		AGameGameState::MakeMatchResultText(/*bEnded=*/true, Teams::Choco).ToString(), TEXT("초코 팀 승리"));

	// 팀 이름의 출처가 하나라는 것. 이름을 바꾸면 문구도 따라와야 한다.
	TestTrue(TEXT("문구의 팀 이름은 Teams::GetDisplayName에서 온다"),
		AGameGameState::MakeMatchResultText(true, Teams::Choco).ToString().Contains(Teams::GetDisplayName(Teams::Choco)));

	TestEqual(TEXT("승팀이 없으면 무승부"),
		AGameGameState::MakeMatchResultText(/*bEnded=*/true, Teams::None).ToString(), TEXT("무승부"));

	// 경기 전에 팝업이 먼저 떠 있어도 "무승부"가 뜨면 안 된다. 승팀 값은 무승부와 같다.
	TestTrue(TEXT("경기가 끝나기 전에는 빈 텍스트"),
		AGameGameState::MakeMatchResultText(/*bEnded=*/false, Teams::None).IsEmpty());
	TestTrue(TEXT("끝나지 않았으면 승팀이 있어도 빈 텍스트"),
		AGameGameState::MakeMatchResultText(/*bEnded=*/false, Teams::Mint).IsEmpty());

	return true;
}

#endif
