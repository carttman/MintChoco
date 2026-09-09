#include "Misc/AutomationTest.h"

#include "Screen/ScreenFadeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 페이드 상태 기계: 어두워지고, 로드 중 고정되고, 준비되면(또는 상한을 넘기면) 밝아진다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FScreenFadeStateTest,
	"MintChoco.Screen.Fade.State",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FScreenFadeStateTest::RunTest(const FString& Parameters)
{
	const float Duration = 0.5f;
	const float MaxWait = 2.0f;

	FScreenFadeState State;
	TestTrue(TEXT("starts clear"), State.IsClear());
	TestEqual(TEXT("starts transparent"), State.Opacity, 0.0f);

	// 페이드 아웃: 0.5초에 걸쳐 1까지.
	State.StartFadeOut();
	TestEqual(TEXT("fading out"), State.Phase, EScreenFadePhase::FadingOut);
	State.Tick(0.25f, Duration, false, MaxWait);
	TestEqual(TEXT("half dark"), State.Opacity, 0.5f, 1e-4f);
	TestFalse(TEXT("not dark yet"), State.IsDark());
	const bool bChanged = State.Tick(0.3f, Duration, false, MaxWait);
	TestTrue(TEXT("phase changed when fully dark"), bChanged);
	TestEqual(TEXT("covered"), State.Phase, EScreenFadePhase::Covered);
	TestEqual(TEXT("clamped at 1"), State.Opacity, 1.0f);

	// Covered는 외부 신호(로드 완료) 없이는 움직이지 않는다.
	State.Tick(5.0f, Duration, true, MaxWait);
	TestEqual(TEXT("covered waits for the map"), State.Phase, EScreenFadePhase::Covered);

	// 로드 완료 → 준비 대기 → 준비되면 페이드 인.
	State.StartWaiting();
	State.Tick(0.1f, Duration, false, MaxWait);
	TestEqual(TEXT("still waiting while not ready"), State.Phase, EScreenFadePhase::WaitingForReady);
	TestEqual(TEXT("still dark while waiting"), State.Opacity, 1.0f);
	State.Tick(0.1f, Duration, true, MaxWait);
	TestEqual(TEXT("ready starts the fade in"), State.Phase, EScreenFadePhase::FadingIn);
	State.Tick(0.25f, Duration, false, MaxWait);
	TestEqual(TEXT("half clear"), State.Opacity, 0.5f, 1e-4f);
	State.Tick(0.25f, Duration, false, MaxWait);
	TestTrue(TEXT("clear again"), State.IsClear());
	TestEqual(TEXT("fully transparent"), State.Opacity, 0.0f);

	// 상한: 준비가 안 돼도 MaxWait이 지나면 연다.
	State.StartWaiting();
	State.Tick(1.9f, Duration, false, MaxWait);
	TestEqual(TEXT("waits up to the limit"), State.Phase, EScreenFadePhase::WaitingForReady);
	State.Tick(0.2f, Duration, false, MaxWait);
	TestEqual(TEXT("opens on timeout"), State.Phase, EScreenFadePhase::FadingIn);

	// 밝아지는 중에 다시 어두워지면 지금 밝기에서 이어진다.
	State.Tick(0.25f, Duration, false, MaxWait);
	const float MidOpacity = State.Opacity;
	State.StartFadeOut();
	TestEqual(TEXT("fade out resumes from the current opacity"), State.Opacity, MidOpacity);
	State.Tick(0.1f, Duration, false, MaxWait);
	TestTrue(TEXT("darker than before"), State.Opacity > MidOpacity);

	// Cover는 즉시 어둡다. 지속시간 0은 한 틱에 끝난다.
	State.Cover();
	TestTrue(TEXT("cover is instant"), State.IsDark() && State.Opacity == 1.0f);
	State.StartWaiting();
	State.Tick(0.0f, 0.0f, true, MaxWait);
	State.Tick(0.0f, 0.0f, false, MaxWait);
	TestTrue(TEXT("zero duration clears in one tick"), State.IsClear());

	// MaxWait 0은 상한 없음.
	State.StartWaiting();
	State.Tick(100.0f, Duration, false, 0.0f);
	TestEqual(TEXT("no limit when MaxWait is 0"), State.Phase, EScreenFadePhase::WaitingForReady);

	return true;
}

#endif
