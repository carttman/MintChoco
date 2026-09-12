#include "Misc/AutomationTest.h"

#include "Game/GameGameState.h"
#include "Game/TeamTypes.h"
#include "Paint/PaintCellGrid.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 팀별 점유 면적이 주어진 커버리지. 전체 면적은 1로 두어 값이 곧 비율이 된다. */
	FPaintCoverage MakeCoverage(float MintFraction, float ChocoFraction)
	{
		FPaintCoverage Coverage;
		Coverage.TotalArea = 1.0f;
		Coverage.AreaByPaintId.SetNumZeroed(FMath::Max(Coverage.AreaByPaintId.Num(), Teams::Count + 1));
		Coverage.AreaByPaintId[Teams::Mint] = MintFraction;
		Coverage.AreaByPaintId[Teams::Choco] = ChocoFraction;
		return Coverage;
	}
}

/** KO 판정의 순수 계산: 누가 기준을 넘었는지와 게이지가 얼마나 찼는지. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKnockoutTest,
	"MintChoco.Match.Knockout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FKnockoutTest::RunTest(const FString& Parameters)
{
	constexpr float Threshold = 0.7f;

	// 아무도 기준에 못 미치면 아무도 안 몰린다.
	TestEqual(TEXT("both below"), FKnockoutMath::LeaderAboveThreshold(MakeCoverage(0.5f, 0.4f), Threshold), Teams::None);

	// 정확히 기준이면 포함한다 - 70 %에서 시작한다고 했으므로 70 %가 경계 안이다.
	TestEqual(TEXT("exactly at threshold counts"), FKnockoutMath::LeaderAboveThreshold(MakeCoverage(0.7f, 0.2f), Threshold), Teams::Mint);
	TestEqual(TEXT("above threshold"), FKnockoutMath::LeaderAboveThreshold(MakeCoverage(0.1f, 0.85f), Threshold), Teams::Choco);

	// 기준을 낮게 잡아 둘이 함께 넘어도 앞선 쪽 하나만 고른다.
	TestEqual(TEXT("two over a low threshold: the leader"), FKnockoutMath::LeaderAboveThreshold(MakeCoverage(0.3f, 0.45f), 0.25f), Teams::Choco);

	// 기준이 0 이하면 KO 자체가 꺼진다. 다 칠해도 안 걸린다.
	TestEqual(TEXT("threshold 0 disables"), FKnockoutMath::LeaderAboveThreshold(MakeCoverage(1.0f, 0.0f), 0.0f), Teams::None);

	// 게이지: 남은 시간이 줄수록 찬다.
	constexpr float Hold = 5.0f;
	TestEqual(TEXT("just started: empty"), FKnockoutMath::Progress(5.0f, Hold), 0.0f);
	TestEqual(TEXT("half way"), FKnockoutMath::Progress(2.5f, Hold), 0.5f);
	TestEqual(TEXT("about to fire: full"), FKnockoutMath::Progress(0.0f, Hold), 1.0f);

	// 시계가 튀어 남은 시간이 범위를 벗어나도 0~1을 넘지 않는다.
	TestEqual(TEXT("overshoot clamps to full"), FKnockoutMath::Progress(-1.0f, Hold), 1.0f);
	TestEqual(TEXT("stale value clamps to empty"), FKnockoutMath::Progress(9.0f, Hold), 0.0f);
	TestEqual(TEXT("zero hold is empty, not a divide by zero"), FKnockoutMath::Progress(1.0f, 0.0f), 0.0f);

	return true;
}

#endif
