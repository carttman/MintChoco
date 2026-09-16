#include "Misc/AutomationTest.h"

#include "Items/PaintRain.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 표식을 몇 개 놓는가. Stride 행마다 하나이고 첫 행에는 언제나 하나 놓이므로, 나눠 올린 값이다.
 * 행 수가 Stride로 나누어떨어지지 않아도 마지막 구간의 표식이 빠지면 안 된다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRainTelegraphCountTest,
	"MintChoco.Items.Rain.TelegraphCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRainTelegraphCountTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("행마다 하나면 행 수 그대로"), FPaintRainPlan::TelegraphCount(10, 1), 10);
	// 1, 4, 7, 10행.
	TestEqual(TEXT("세 행마다 하나"), FPaintRainPlan::TelegraphCount(10, 3), 4);
	// 1, 6행.
	TestEqual(TEXT("다섯 행마다 하나"), FPaintRainPlan::TelegraphCount(10, 5), 2);
	TestEqual(TEXT("행보다 큰 간격이면 첫 행 하나"), FPaintRainPlan::TelegraphCount(10, 100), 1);

	TestEqual(TEXT("행이 없으면 표식도 없다"), FPaintRainPlan::TelegraphCount(0, 3), 0);
	// 에디터에서 막고 있지만, 0이 들어와도 나누기에서 터지지 않아야 한다.
	TestEqual(TEXT("간격 0은 1로 친다"), FPaintRainPlan::TelegraphCount(10, 0), 10);
	TestEqual(TEXT("음수 간격도 1로 친다"), FPaintRainPlan::TelegraphCount(10, -2), 10);

	return true;
}

/**
 * 예고 표식의 간격. 마지막 표식이 놓이는 순간 첫 행이 떨어져야 예고가 폭격을 한 걸음 앞서
 * 훑고 지나가는 그림이 된다. 그래서 리드인을 표식 수로 나눈다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRainTelegraphIntervalTest,
	"MintChoco.Items.Rain.TelegraphInterval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRainTelegraphIntervalTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("1초를 표식 10개에 나눈다"), FPaintRainPlan::TelegraphInterval(1.0f, 10), 0.1f, 1e-4f);
	TestEqual(TEXT("표식이 하나면 리드인 전체"), FPaintRainPlan::TelegraphInterval(1.0f, 1), 1.0f, 1e-4f);

	// 리드인이 없으면 예고도 없다: 간격을 0으로 돌려 호출부가 타이머를 걸지 않게 한다.
	TestEqual(TEXT("리드인이 없으면 0"), FPaintRainPlan::TelegraphInterval(0.0f, 10), 0.0f);
	TestEqual(TEXT("표식이 없으면 0"), FPaintRainPlan::TelegraphInterval(1.0f, 0), 0.0f);
	TestEqual(TEXT("음수도 0"), FPaintRainPlan::TelegraphInterval(1.0f, -3), 0.0f);

	return true;
}

/**
 * 액터 수명. 리드인이 붙었으므로 그만큼 더 살아야 한다 — 예전 식대로면 리드인이 긴 폭격이
 * 행을 다 떨어뜨리기 전에 사라진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRainLifespanTest,
	"MintChoco.Items.Rain.Lifespan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRainLifespanTest::RunTest(const FString& Parameters)
{
	FPaintRainParams Params;
	Params.RowCount = 10;
	Params.Interval = 0.05f;

	Params.LeadInSeconds = 0.0f;
	TestEqual(TEXT("리드인이 없으면 예전과 같다"), FPaintRainPlan::Lifespan(Params), 2.5f, 1e-4f);

	Params.LeadInSeconds = 1.0f;
	TestEqual(TEXT("리드인만큼 더 산다"), FPaintRainPlan::Lifespan(Params), 3.5f, 1e-4f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
