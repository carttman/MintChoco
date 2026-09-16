#include "Misc/AutomationTest.h"

#include "Game/Unit.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 몸에 덧씌우는 껍데기(슈퍼아머 테두리, 스피드 스타 오라)를 언제 보이는가.
 *
 * 상태가 서 있는 것만으로는 부족하다: 카메라가 몸 안에 들어와 본체가 반투명해진 동안에는
 * 껍데기를 감춰야 한다. 껍데기는 불투명이라 그대로 두면 페이드된 몸 위에 실루엣만 둥둥 뜬다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitShellVisibilityTest,
	"MintChoco.Game.Unit.ShellVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnitShellVisibilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("상태가 서 있고 페이드가 없으면 보인다"), AUnit::ShouldShowShell(true, false));
	TestFalse(TEXT("페이드 중에는 상태가 서 있어도 감춘다"), AUnit::ShouldShowShell(true, true));
	TestFalse(TEXT("상태가 없으면 보이지 않는다"), AUnit::ShouldShowShell(false, false));
	TestFalse(TEXT("상태도 없고 페이드 중이면 당연히 감춘다"), AUnit::ShouldShowShell(false, true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
