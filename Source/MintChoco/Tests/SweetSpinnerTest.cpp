#include "Misc/AutomationTest.h"

#include "Animation/AnimSequence.h"

#include "Items/SpinnerVolleyWindow.h"
#include "Items/SweetSpinnerProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 스위트 스피너의 산탄 방향: 시작 요에서 출발해 정해진 바퀴 수를 발 수로 고르게 나눈다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSweetSpinnerYawTest,
	"MintChoco.Items.SweetSpinner.Yaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSweetSpinnerYawTest::RunTest(const FString& Parameters)
{
	// 3바퀴를 20발로: 한 발에 54도.
	TestEqual(TEXT("first volley faces the start"), SweetSpinner::VolleyYawDegrees(90.0f, 0, 20, 3.0f), 90.0f, 1e-3f);
	TestEqual(TEXT("second volley is one step on"), SweetSpinner::VolleyYawDegrees(90.0f, 1, 20, 3.0f), 90.0f + 54.0f, 1e-3f);
	TestEqual(TEXT("last volley is one step short of the full turns"), SweetSpinner::VolleyYawDegrees(0.0f, 19, 20, 3.0f), 19.0f * 54.0f, 1e-3f);

	// 발 수가 하나면 시작 방향으로 한 발.
	TestEqual(TEXT("single volley"), SweetSpinner::VolleyYawDegrees(30.0f, 0, 1, 3.0f), 30.0f, 1e-3f);

	// 바퀴 수 0이면 전부 같은 방향.
	TestEqual(TEXT("no turns: all forward"), SweetSpinner::VolleyYawDegrees(10.0f, 7, 20, 0.0f), 10.0f, 1e-3f);

	// 프로필의 발 수: 지속 2초 / 간격 0.1초 = 20발.
	USweetSpinnerProfile* const Profile = NewObject<USweetSpinnerProfile>();
	Profile->Duration = 2.0f;
	Profile->VolleyInterval = 0.1f;
	TestEqual(TEXT("volley count"), Profile->GetVolleyCount(), 20);

	return true;
}

/**
 * 발사 구간을 애니메이션이 정한다: 시퀀스에 얹은 Spinner Volley Window 동안만 쏜다.
 *
 * 런타임 노티파이가 아니라 에셋에 박힌 데이터를 읽는 것이 핵심이다. 데디케이티드 서버는
 * 메시의 포즈를 돌리지 않아 노티파이가 오지 않는데, 페인트는 서버가 찍어야 하기 때문이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSweetSpinnerVolleyWindowTest,
	"MintChoco.Items.SweetSpinner.VolleyWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSweetSpinnerVolleyWindowTest::RunTest(const FString& Parameters)
{
	float Start = -1.0f;
	float End = -1.0f;

	// 시퀀스가 없으면 구간도 없다.
	TestFalse(TEXT("시퀀스가 없으면 구간이 없다"), SweetSpinner::FindVolleyWindow(nullptr, Start, End));

	// 표시가 없는 시퀀스도 마찬가지. 다른 노티파이가 있어도 이 구간으로 치지 않는다.
	UAnimSequence* const Bare = NewObject<UAnimSequence>();
	FAnimNotifyEvent Point;
	Point.SetTime(0.3f);
	Bare->Notifies.Add(Point);
	TestFalse(TEXT("표시가 없으면 구간이 없다"), SweetSpinner::FindVolleyWindow(Bare, Start, End));

	// 표시가 있으면 그 시작과 끝을 읽는다.
	UAnimSequence* const Marked = NewObject<UAnimSequence>();
	FAnimNotifyEvent Window;
	Window.NotifyStateClass = NewObject<UAnimNotifyState_SpinnerVolley>(Marked);
	Window.SetTime(0.4f);
	Window.SetDuration(1.2f);
	Marked->Notifies.Add(Window);

	if (TestTrue(TEXT("표시한 구간을 찾는다"), SweetSpinner::FindVolleyWindow(Marked, Start, End)))
	{
		TestEqual(TEXT("구간의 시작"), Start, 0.4f, 1e-3f);
		TestEqual(TEXT("구간의 끝"), End, 1.6f, 1e-3f);
	}


	return true;
}

/**
 * 세 구간(시작 → 회전 → 끝)에서 산탄이 나가는 구간을 어떻게 고르는가.
 *
 * 경계는 각 클립의 길이가 정한다. 준비 동작 동안에는 쏘지 않아야 하고, 효과가 끝난 뒤에도
 * 쏘면 안 된다 — 그 순간 상태 태그가 내려가 자세도 함께 풀리기 때문이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSweetSpinnerPhaseTest,
	"MintChoco.Items.SweetSpinner.Phases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSweetSpinnerPhaseTest::RunTest(const FString& Parameters)
{
	float Start = 0.0f;
	float End = 0.0f;

	// 시작 0.5초, 회전 2초, 지속 3초. 표시가 없으면 회전 구간 전체에서 쏜다.
	SweetSpinner::ComposeVolleyWindow(0.5f, 2.0f, 0.0f, 2.0f, 3.0f, Start, End);
	TestEqual(TEXT("준비 동작이 끝난 뒤에 쏜다"), Start, 0.5f, 1e-3f);
	TestEqual(TEXT("회전이 끝나면 멈춘다"), End, 2.5f, 1e-3f);

	// 시작 동작이 없으면 곧바로 회전부터.
	SweetSpinner::ComposeVolleyWindow(0.0f, 2.0f, 0.0f, 2.0f, 3.0f, Start, End);
	TestEqual(TEXT("시작 동작이 없으면 처음부터"), Start, 0.0f, 1e-3f);
	TestEqual(TEXT("회전 길이만큼 쏜다"), End, 2.0f, 1e-3f);

	// 회전 클립 안의 표시는 그 클립 기준이라 시작 길이만큼 밀린다.
	SweetSpinner::ComposeVolleyWindow(0.5f, 2.0f, 0.4f, 1.6f, 3.0f, Start, End);
	TestEqual(TEXT("표시는 회전 구간 안에서 잰다"), Start, 0.9f, 1e-3f);
	TestEqual(TEXT("표시한 자리에서 끝난다"), End, 2.1f, 1e-3f);

	// 표시가 회전 클립보다 길면 클립 안으로 자른다.
	SweetSpinner::ComposeVolleyWindow(0.5f, 1.0f, 0.0f, 5.0f, 3.0f, Start, End);
	TestEqual(TEXT("표시는 회전 클립을 넘지 못한다"), End, 1.5f, 1e-3f);

	// 이게 핵심: 지속시간 밖으로는 나가지 않는다.
	SweetSpinner::ComposeVolleyWindow(0.5f, 2.0f, 0.0f, 2.0f, 1.0f, Start, End);
	TestEqual(TEXT("지속시간 밖은 잘린다"), End, 1.0f, 1e-3f);
	TestTrue(TEXT("잘려도 구간이 뒤집히지 않는다"), Start <= End);

	// 준비 동작이 지속시간보다 길면 쏠 틈이 없다. 빈 구간이지 거꾸로 된 구간이 아니다.
	SweetSpinner::ComposeVolleyWindow(4.0f, 2.0f, 0.0f, 2.0f, 3.0f, Start, End);
	TestEqual(TEXT("쏠 틈이 없으면 빈 구간"), Start, 3.0f, 1e-3f);
	TestEqual(TEXT("빈 구간의 끝은 시작과 같다"), End, 3.0f, 1e-3f);

	// 프로필: 클립이 하나도 없으면 지속시간 전체가 회전 구간이다(예전 동작).
	USweetSpinnerProfile* const Profile = NewObject<USweetSpinnerProfile>();
	Profile->Duration = 2.0f;
	Profile->VolleyInterval = 0.1f;
	Profile->GetVolleyWindow(Start, End);
	TestEqual(TEXT("클립이 없으면 처음부터"), Start, 0.0f, 1e-3f);
	TestEqual(TEXT("클립이 없으면 끝까지"), End, 2.0f, 1e-3f);
	TestEqual(TEXT("발 수는 구간 길이가 정한다"), Profile->GetVolleyCount(End - Start), 20);

	return true;
}

/**
 * 끝 동작은 효과 밖의 마무리다. 태그는 회전이 끝나는 시각에 내려가야 끝 동작 중에 총과 아이템이
 * 바로 나가고, 마무리는 끝 클립 한 번이며 지속시간을 넘지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSweetSpinnerRecoveryTest,
	"MintChoco.Items.SweetSpinner.Recovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSweetSpinnerRecoveryTest::RunTest(const FString& Parameters)
{
	float Effect = 0.0f;
	float Recovery = 0.0f;

	// 시작 0.5 + 회전 2.0 = 2.5에 태그가 내려가고, 끝 클립 0.8이 남은 0.8에 딱 맞는다.
	SweetSpinner::ComposeRecovery(0.5f, 2.0f, 0.8f, 3.3f, Effect, Recovery);
	TestEqual(TEXT("태그는 회전이 끝나는 시각까지"), Effect, 2.5f, 1e-3f);
	TestEqual(TEXT("마무리는 끝 클립 길이"), Recovery, 0.8f, 1e-3f);

	// 남은 시간이 끝 클립보다 짧으면 지속시간에서 잘린다(예전에도 거기서 잘렸다).
	SweetSpinner::ComposeRecovery(0.5f, 2.0f, 0.8f, 3.0f, Effect, Recovery);
	TestEqual(TEXT("짧아도 태그는 회전 끝까지"), Effect, 2.5f, 1e-3f);
	TestEqual(TEXT("마무리는 지속시간 안으로 잘린다"), Recovery, 0.5f, 1e-3f);

	// 남은 시간이 더 길어도 끝 클립은 한 번만 돈다. 늘이면 루프하는 자세가 되감긴다.
	SweetSpinner::ComposeRecovery(0.5f, 1.0f, 0.5f, 3.0f, Effect, Recovery);
	TestEqual(TEXT("길어도 태그는 회전 끝까지"), Effect, 1.5f, 1e-3f);
	TestEqual(TEXT("끝 클립은 한 번"), Recovery, 0.5f, 1e-3f);

	// 끝 클립이 없으면 예전 동작: 효과가 지속시간 전체, 마무리 없음.
	SweetSpinner::ComposeRecovery(0.5f, 2.0f, 0.0f, 3.0f, Effect, Recovery);
	TestEqual(TEXT("끝 동작이 없으면 효과가 끝까지"), Effect, 3.0f, 1e-3f);
	TestEqual(TEXT("끝 동작이 없으면 마무리 없음"), Recovery, 0.0f);

	// 회전이 지속시간을 다 쓰면 끝 동작이 들어갈 틈이 없다.
	SweetSpinner::ComposeRecovery(0.5f, 3.0f, 0.8f, 3.0f, Effect, Recovery);
	TestEqual(TEXT("틈이 없으면 효과가 끝까지"), Effect, 3.0f, 1e-3f);
	TestEqual(TEXT("틈이 없으면 마무리 없음"), Recovery, 0.0f);

	// 회전 구간이 비어 있으면(클립 길이 0) 태그를 0초로 걸 수 없으니 예전 동작으로 둔다.
	SweetSpinner::ComposeRecovery(0.0f, 0.0f, 0.8f, 3.0f, Effect, Recovery);
	TestEqual(TEXT("회전이 비면 효과가 끝까지"), Effect, 3.0f, 1e-3f);
	TestEqual(TEXT("회전이 비면 마무리 없음"), Recovery, 0.0f);

	// 합은 절대 지속시간을 넘지 않는다.
	for (const float Duration : {1.0f, 2.5f, 3.0f, 4.0f})
	{
		SweetSpinner::ComposeRecovery(0.5f, 2.0f, 0.8f, Duration, Effect, Recovery);
		TestTrue(*FString::Printf(TEXT("지속 %.1f초: 효과 + 마무리가 넘지 않는다"), Duration), Effect + Recovery <= Duration + 1e-3f);
	}

	// 프로필: 클립이 하나도 없으면 효과가 지속시간 전체이고 마무리가 없다.
	USweetSpinnerProfile* const Profile = NewObject<USweetSpinnerProfile>();
	Profile->Duration = 2.0f;
	TestEqual(TEXT("클립이 없는 프로필은 효과가 끝까지"), Profile->GetEffectPhaseLength(), 2.0f, 1e-3f);
	TestEqual(TEXT("클립이 없는 프로필은 마무리 없음"), Profile->GetEndPhaseLength(), 0.0f);

	return true;
}

#endif
