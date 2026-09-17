#include "Misc/AutomationTest.h"

#include "Game/BobMotion.h"
#include "Items/ItemPickup.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 아이템 박스 연출: 요는 시간에 비례하며 정규화된다. 상하 흔들림은 BobMotion 쪽에서 본다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemPickupMotionTest,
	"MintChoco.Items.Pickup.Motion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemPickupMotionTest::RunTest(const FString& Parameters)
{
	// 요: 속도 × 시간, -180~180으로 감는다. 음수 속도는 반대로 돈다.
	TestEqual(TEXT("spin starts at zero"), FItemPickupMotion::SpinYaw(0.0f, 90.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("spin after one second"), FItemPickupMotion::SpinYaw(1.0f, 90.0f), 90.0f, 1e-3f);
	TestEqual(TEXT("spin wraps past 180"), FItemPickupMotion::SpinYaw(3.0f, 90.0f), -90.0f, 1e-3f);
	TestEqual(TEXT("spin wraps a full turn to zero"), FItemPickupMotion::SpinYaw(4.0f, 90.0f), 0.0f, 1e-3f);
	TestEqual(TEXT("negative rate spins the other way"), FItemPickupMotion::SpinYaw(1.0f, -90.0f), -90.0f, 1e-3f);

	return true;
}

/**
 * 둥둥 떠 있는 연출의 계산. 아이템 상자와 풍선이 같이 쓴다.
 *
 * 위상이 있는 이유는 풍선 때문이다: 레벨에 놓인 것들은 전부 같은 프레임에 시작하므로, 위상이
 * 없으면 맵의 풍선이 한 몸처럼 같이 오르내린다. 자리에서 뽑으므로 모든 머신이 같은 값을 내고
 * 터졌다 다시 부풀어도 위상이 튀지 않는다 — 시각에서 뽑으면 둘 다 깨진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBobMotionTest,
	"MintChoco.Game.BobMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBobMotionTest::RunTest(const FString& Parameters)
{
	// 상하: 시작은 0, 1/4 주기에서 +진폭, 3/4 주기에서 -진폭, 한 주기 뒤 다시 0.
	TestEqual(TEXT("bob starts level"), BobMotion::Offset(0.0f, 10.0f, 1.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("bob peaks at a quarter period"), BobMotion::Offset(0.25f, 10.0f, 1.0f), 10.0f, 1e-3f);
	TestEqual(TEXT("bob dips at three quarters"), BobMotion::Offset(0.75f, 10.0f, 1.0f), -10.0f, 1e-3f);
	TestEqual(TEXT("bob returns after a period"), BobMotion::Offset(1.0f, 10.0f, 1.0f), 0.0f, 1e-3f);
	TestEqual(TEXT("bob follows the frequency"), BobMotion::Offset(0.125f, 10.0f, 2.0f), 10.0f, 1e-3f);
	TestEqual(TEXT("no amplitude: no bob"), BobMotion::Offset(0.25f, 0.0f, 1.0f), 0.0f);
	TestEqual(TEXT("no frequency: no bob"), BobMotion::Offset(0.25f, 10.0f, 0.0f), 0.0f);

	// 위상은 주기 안의 자리다. 1/4을 주면 시작부터 꼭대기이고, 한 바퀴는 위상이 없는 것과 같다.
	TestEqual(TEXT("a quarter phase starts at the peak"), BobMotion::Offset(0.0f, 10.0f, 1.0f, 0.25f), 10.0f, 1e-3f);
	TestEqual(TEXT("a full phase is the same as none"), BobMotion::Offset(0.3f, 10.0f, 1.0f, 1.0f), BobMotion::Offset(0.3f, 10.0f, 1.0f), 1e-3f);
	TestEqual(TEXT("phase shifts the whole curve"), BobMotion::Offset(0.25f, 10.0f, 1.0f, 0.5f), -10.0f, 1e-3f);
	TestEqual(TEXT("phase does not wake a still bob"), BobMotion::Offset(0.25f, 0.0f, 1.0f, 0.25f), 0.0f);

	// 자리에서 뽑은 위상: 같은 자리면 언제나 같다. 다시 부풀 때 위상이 튀지 않는 근거다.
	const FVector Here(1200.0f, -400.0f, 250.0f);
	TestEqual(TEXT("the same spot always gives the same phase"),
		BobMotion::PhaseFromVector(Here), BobMotion::PhaseFromVector(Here));

	// 그리고 0~1 안에 있다. 벗어나면 위상이 아니라 시간 이동이 된다.
	for (const FVector& Spot : { FVector::ZeroVector, Here, FVector(-9999.0f, 9999.0f, 0.0f), FVector(0.4f, -0.4f, 0.6f) })
	{
		const float Phase = BobMotion::PhaseFromVector(Spot);
		TestTrue(*FString::Printf(TEXT("%s의 위상 %.4f가 0~1 안에 있다"), *Spot.ToString(), Phase),
			Phase >= 0.0f && Phase < 1.0f);
	}

	// 이게 이 함수를 만든 이유다: 나란히 놓인 풍선들이 서로 다른 위상을 받는다. 해시라 어쩌다
	// 겹칠 수는 있으므로 "전부 다르다"가 아니라 "거의 다 다르다"로 본다.
	TSet<float> Phases;
	constexpr int32 Count = 16;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Phases.Add(BobMotion::PhaseFromVector(FVector(Index * 300.0f, 0.0f, 0.0f)));
	}
	TestTrue(*FString::Printf(TEXT("나란한 %d개가 %d가지 위상을 받는다"), Count, Phases.Num()), Phases.Num() >= Count - 1);

	return true;
}

#endif
