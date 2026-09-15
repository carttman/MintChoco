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
	// HUD 바와 같은 자: 합이 60 % 전이면 60 % 로, 넘으면 합으로 나눈 몫이 게이지이고, 선은 양 끝에서 30 % 자리다.
	constexpr float Clash = 0.6f;
	constexpr float Line = 0.3f;

	// 몫이 둘 다 70 % 에 못 미치면 아무도 안 몰린다. 0.5 : 0.4 는 0.56 : 0.44 다.
	TestEqual(TEXT("both below"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.5f, 0.4f), Clash, Line), Teams::None);

	// 정확히 선이면 포함한다. 0.56 : 0.24 는 몫 0.7 이다.
	TestEqual(TEXT("exactly at the line counts"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.56f, 0.24f), Clash, Line), Teams::Mint);
	TestEqual(TEXT("past the line"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.1f, 0.85f), Clash, Line), Teams::Choco);

	// 격돌 전에도 몰아붙일 수 있다. 0.45 / 0.6 = 0.75.
	TestEqual(TEXT("dominant before the clash"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.1f, 0.45f), Clash, Line), Teams::Choco);
	TestEqual(TEXT("66 % of the map is not enough while the other side holds 34 %"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.66f, 0.34f), Clash, Line), Teams::None);

	// 선을 낮게 잡아 둘이 함께 넘어도 앞선 쪽 하나만 고른다. 0.45 : 0.5 는 0.47 : 0.53 이고 선은 0.4 다.
	TestEqual(TEXT("two over a low line: the leader"), FKnockoutMath::LeaderPastLine(MakeCoverage(0.45f, 0.5f), Clash, 0.6f), Teams::Choco);

	// 선이 0 이하면 KO 자체가 꺼진다. 다 칠해도 안 걸린다.
	TestEqual(TEXT("line 0 disables"), FKnockoutMath::LeaderPastLine(MakeCoverage(1.0f, 0.0f), Clash, 0.0f), Teams::None);

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
