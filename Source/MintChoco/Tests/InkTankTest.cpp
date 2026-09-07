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

#endif
