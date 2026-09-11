#include "Misc/AutomationTest.h"

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

#endif
