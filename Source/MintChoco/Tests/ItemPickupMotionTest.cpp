#include "Misc/AutomationTest.h"

#include "Items/ItemPickup.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 아이템 박스 연출: 상하는 사인파, 요는 시간에 비례하며 정규화된다. 0 진폭이나 0 주파수는 움직이지 않는다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemPickupMotionTest,
	"MintChoco.Items.Pickup.Motion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemPickupMotionTest::RunTest(const FString& Parameters)
{
	// 상하: 시작은 0, 1/4 주기에서 +진폭, 3/4 주기에서 -진폭, 한 주기 뒤 다시 0.
	TestEqual(TEXT("bob starts level"), FItemPickupMotion::BobOffset(0.0f, 10.0f, 1.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("bob peaks at a quarter period"), FItemPickupMotion::BobOffset(0.25f, 10.0f, 1.0f), 10.0f, 1e-3f);
	TestEqual(TEXT("bob dips at three quarters"), FItemPickupMotion::BobOffset(0.75f, 10.0f, 1.0f), -10.0f, 1e-3f);
	TestEqual(TEXT("bob returns after a period"), FItemPickupMotion::BobOffset(1.0f, 10.0f, 1.0f), 0.0f, 1e-3f);
	TestEqual(TEXT("bob follows the frequency"), FItemPickupMotion::BobOffset(0.125f, 10.0f, 2.0f), 10.0f, 1e-3f);
	TestEqual(TEXT("no amplitude: no bob"), FItemPickupMotion::BobOffset(0.25f, 0.0f, 1.0f), 0.0f);
	TestEqual(TEXT("no frequency: no bob"), FItemPickupMotion::BobOffset(0.25f, 10.0f, 0.0f), 0.0f);

	// 요: 속도 × 시간, -180~180으로 감는다. 음수 속도는 반대로 돈다.
	TestEqual(TEXT("spin starts at zero"), FItemPickupMotion::SpinYaw(0.0f, 90.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("spin after one second"), FItemPickupMotion::SpinYaw(1.0f, 90.0f), 90.0f, 1e-3f);
	TestEqual(TEXT("spin wraps past 180"), FItemPickupMotion::SpinYaw(3.0f, 90.0f), -90.0f, 1e-3f);
	TestEqual(TEXT("spin wraps a full turn to zero"), FItemPickupMotion::SpinYaw(4.0f, 90.0f), 0.0f, 1e-3f);
	TestEqual(TEXT("negative rate spins the other way"), FItemPickupMotion::SpinYaw(1.0f, -90.0f), -90.0f, 1e-3f);

	return true;
}

#endif
