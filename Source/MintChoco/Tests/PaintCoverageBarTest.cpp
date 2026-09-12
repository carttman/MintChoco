#include "Misc/AutomationTest.h"

#include "Paint/PaintCoverageBarWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 게이지 두 칸의 폭. 절대 점유율이므로 합이 1보다 작으면 가운데가 비고, 합이 1을 넘기려 하면
 * 먼저 들어온 민트가 자리를 지키고 초코가 남은 만큼만 차지한다(복제가 따라오는 동안의 과도 상태).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCoverageBarFillsTest,
	"MintChoco.Paint.Coverage.BarFills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCoverageBarFillsTest::RunTest(const FString& Parameters)
{
	float Mint = 0.0f;
	float Choco = 0.0f;

	// 둘 다 조금 칠한 상태: 가운데가 남는다.
	FPaintCoverageBarMath::ComputeFills(0.2f, 0.15f, Mint, Choco);
	TestEqual(TEXT("mint keeps its share"), Mint, 0.2f, 1e-4f);
	TestEqual(TEXT("choco keeps its share"), Choco, 0.15f, 1e-4f);
	TestTrue(TEXT("the middle is still bare"), Mint + Choco < 1.0f);

	// 만나는 지점은 점유율이 많은 쪽으로 밀린다: 민트가 더 많으면 경계가 오른쪽으로 간다.
	FPaintCoverageBarMath::ComputeFills(0.6f, 0.4f, Mint, Choco);
	TestEqual(TEXT("the meeting point sits at the mint share"), Mint, 0.6f, 1e-4f);
	TestEqual(TEXT("choco fills the rest"), Choco, 0.4f, 1e-4f);

	// 합이 1을 넘는 과도 상태에서도 두 칸은 겹치지 않는다.
	FPaintCoverageBarMath::ComputeFills(0.7f, 0.5f, Mint, Choco);
	TestEqual(TEXT("mint is unchanged"), Mint, 0.7f, 1e-4f);
	TestEqual(TEXT("choco is cut to the space left"), Choco, 0.3f, 1e-4f);
	TestTrue(TEXT("the two fills never overlap"), Mint + Choco <= 1.0f + 1e-4f);

	// 음수와 1 초과는 잘린다.
	FPaintCoverageBarMath::ComputeFills(-0.5f, 2.0f, Mint, Choco);
	TestEqual(TEXT("a negative share is zero"), Mint, 0.0f, 1e-4f);
	TestEqual(TEXT("an over-full share is one"), Choco, 1.0f, 1e-4f);

	// 아무것도 안 칠한 상태.
	FPaintCoverageBarMath::ComputeFills(0.0f, 0.0f, Mint, Choco);
	TestEqual(TEXT("empty stays empty"), Mint, 0.0f, 1e-4f);
	TestEqual(TEXT("empty stays empty"), Choco, 0.0f, 1e-4f);

	return true;
}

#endif
