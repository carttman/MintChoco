#include "Misc/AutomationTest.h"

#include "Ink/InkTankComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInkTankSpendAndRefillTest,
	"MintChoco.Ink.Tank.SpendAndRefill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInkTankSpendAndRefillTest::RunTest(const FString& Parameters)
{
	UInkTankComponent* const Tank = NewObject<UInkTankComponent>();
	Tank->RefillPerSecond = 0.5f;
	Tank->RefillDelayAfterSpend = 1.0f;

	TestEqual(TEXT("starts full"), Tank->GetInk(), 1.0f);
	TestTrue(TEXT("spends what it holds"), Tank->TryConsume(0.3f));
	TestEqual(TEXT("after one spend"), Tank->GetInk(), 0.7f, 1e-4f);
	TestFalse(TEXT("refuses more than it holds"), Tank->TryConsume(0.8f));
	TestEqual(TEXT("a refused spend costs nothing"), Tank->GetInk(), 0.7f, 1e-4f);

	Tank->Refill(0.5f);
	TestEqual(TEXT("no refill inside the pause"), Tank->GetInk(), 0.7f, 1e-4f);
	Tank->Refill(1.0f);
	TestEqual(TEXT("refills for the time past the pause"), Tank->GetInk(), 0.95f, 1e-4f);
	Tank->Refill(10.0f);
	TestEqual(TEXT("clamps at full"), Tank->GetInk(), 1.0f);

	Tank->SetInk(0.0f);
	TestFalse(TEXT("empty affords nothing"), Tank->CanAfford(0.01f));
	TestTrue(TEXT("empty still affords a free shot"), Tank->CanAfford(0.0f));
	Tank->SetInk(5.0f);
	TestEqual(TEXT("SetInk clamps"), Tank->GetInk(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInkTankDashRefillTest,
	"MintChoco.Ink.Tank.DashRefill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInkTankDashRefillTest::RunTest(const FString& Parameters)
{
	UInkTankComponent* const Tank = NewObject<UInkTankComponent>();
	Tank->RefillPerSecond = 0.1f;
	Tank->RefillDelayAfterSpend = 0.0f;
	Tank->DashRefillMultiplier = 3.0f;

	TestEqual(TEXT("평소 속도"), Tank->GetRefillPerSecond(), 0.1f, 1e-4f);
	Tank->SetInk(0.0f);
	Tank->Refill(1.0f);
	TestEqual(TEXT("평소에는 배율이 붙지 않는다"), Tank->GetInk(), 0.1f, 1e-4f);

	Tank->SetDashing(true);
	TestEqual(TEXT("보드 위의 속도"), Tank->GetRefillPerSecond(), 0.3f, 1e-4f);
	Tank->SetInk(0.0f);
	Tank->Refill(1.0f);
	TestEqual(TEXT("보드 위에서는 배율만큼 찬다"), Tank->GetInk(), 0.3f, 1e-4f);

	// 보드에서 내리면 그 자리에서 평소 속도로 돌아간다. 남은 배율이 따라오면 대시를 톡톡
	// 끊어 치는 것만으로 계속 빠르게 찬다.
	Tank->SetDashing(false);
	Tank->SetInk(0.0f);
	Tank->Refill(1.0f);
	TestEqual(TEXT("내리면 평소 속도"), Tank->GetInk(), 0.1f, 1e-4f);

	// 1보다 작은 배율은 보드가 회복을 늦추는 뜻이 되므로 막혀 있다.
	Tank->DashRefillMultiplier = 0.0f;
	Tank->SetDashing(true);
	TestEqual(TEXT("1보다 작은 배율은 1로 본다"), Tank->GetRefillPerSecond(), 0.1f, 1e-4f);

	// 쓴 직후의 멈춤은 보드를 타도 그대로다.
	Tank->DashRefillMultiplier = 3.0f;
	Tank->RefillDelayAfterSpend = 1.0f;
	Tank->SetInk(1.0f);
	TestTrue(TEXT("한 발 쓴다"), Tank->TryConsume(0.5f));
	Tank->Refill(0.5f);
	TestEqual(TEXT("멈춤 안에서는 보드도 채우지 않는다"), Tank->GetInk(), 0.5f, 1e-4f);
	Tank->Refill(1.0f);
	TestEqual(TEXT("멈춤이 끝난 만큼만 배율로 찬다"), Tank->GetInk(), 0.5f + 0.3f * 0.5f, 1e-4f);
	return true;
}

#endif
