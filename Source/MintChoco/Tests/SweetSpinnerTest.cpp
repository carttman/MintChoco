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

	// 프로필: 표시가 없으면 지속시간 전체가 구간이다(예전 동작).
	USweetSpinnerProfile* const Profile = NewObject<USweetSpinnerProfile>();
	Profile->Duration = 2.0f;
	Profile->VolleyInterval = 0.1f;
	Profile->GetVolleyWindow(Start, End);
	TestEqual(TEXT("표시가 없으면 처음부터"), Start, 0.0f, 1e-3f);
	TestEqual(TEXT("표시가 없으면 끝까지"), End, 2.0f, 1e-3f);
	TestEqual(TEXT("발 수는 구간 전체"), Profile->GetVolleyCount(End - Start), 20);

	// 표시가 있으면 그 구간만. 준비 동작 동안에는 쏘지 않는다.
	Profile->SpinAnimation = Marked;
	Profile->GetVolleyWindow(Start, End);
	TestEqual(TEXT("표시한 만큼 늦게 시작한다"), Start, 0.4f, 1e-3f);
	TestEqual(TEXT("표시한 자리에서 끝난다"), End, 1.6f, 1e-3f);
	TestEqual(TEXT("발 수도 구간에 맞춰 줄어든다"), Profile->GetVolleyCount(End - Start), 12);

	// 구간이 지속시간을 넘으면 잘라 낸다. 효과가 끝난 뒤에는 쏠 수 없다.
	Profile->Duration = 1.0f;
	Profile->GetVolleyWindow(Start, End);
	TestEqual(TEXT("지속시간 밖은 잘린다"), End, 1.0f, 1e-3f);

	return true;
}

#endif
