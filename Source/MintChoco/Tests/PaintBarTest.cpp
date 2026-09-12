#include "Misc/AutomationTest.h"

#include "Game/PaintBar.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 칠한 비율 바의 게이지 길이, KO 판정선, KO 시계, KO 과장, 미리보기 곡선. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintBarTest,
	"MintChoco.Match.PaintBar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintBarTest::RunTest(const FString& Parameters)
{
	const FPaintBarRules Rules;
	const float Tolerance = 1.0e-3f;

	// 게이지 길이: 합이 60%에 닿기 전에는 60%로, 넘으면 합으로 나눈다.
	{
		const FPaintBarFill Empty = FPaintBarMath::ComputeFill(0.0f, 0.0f, Rules.ClashCoverage);
		TestEqual(TEXT("empty left"), Empty.Left, 0.0f);
		TestEqual(TEXT("empty right"), Empty.Right, 0.0f);
		TestFalse(TEXT("empty does not clash"), Empty.IsClashing());

		const FPaintBarFill Early = FPaintBarMath::ComputeFill(0.1f, 0.2f, Rules.ClashCoverage);
		TestEqual(TEXT("early left fills from its end"), Early.Left, 0.1667f, Tolerance);
		TestEqual(TEXT("early right fills from its end"), Early.Right, 0.3333f, Tolerance);
		TestFalse(TEXT("early leaves a gap"), Early.IsClashing());

		const FPaintBarFill Meet = FPaintBarMath::ComputeFill(0.23f, 0.37f, Rules.ClashCoverage);
		TestEqual(TEXT("23:37 left"), Meet.Left, 0.3833f, Tolerance);
		TestEqual(TEXT("23:37 right"), Meet.Right, 0.6167f, Tolerance);
		TestTrue(TEXT("60% meets"), Meet.IsClashing());

		const FPaintBarFill Late = FPaintBarMath::ComputeFill(0.5f, 0.4f, Rules.ClashCoverage);
		TestEqual(TEXT("late left follows the ratio"), Late.Left, 0.5556f, Tolerance);
		TestEqual(TEXT("late right follows the ratio"), Late.Right, 0.4444f, Tolerance);
		TestTrue(TEXT("late clashes"), Late.IsClashing());

		const FPaintBarFill Below = FPaintBarMath::ComputeFill(0.2999f, 0.3f, Rules.ClashCoverage);
		const FPaintBarFill Above = FPaintBarMath::ComputeFill(0.3001f, 0.3f, Rules.ClashCoverage);
		TestEqual(TEXT("left is continuous at the clash point"), Below.Left, Above.Left, Tolerance);
		TestEqual(TEXT("right is continuous at the clash point"), Below.Right, Above.Right, Tolerance);

		const FPaintBarFill Alone = FPaintBarMath::ComputeFill(0.0f, 0.7f, Rules.ClashCoverage);
		TestEqual(TEXT("alone fills the whole bar"), Alone.Right, 1.0f, Tolerance);
		TestFalse(TEXT("alone has nobody to clash with"), Alone.IsClashing());
	}

	// KO 판정선: 상대 게이지가 선에 닿으면 넘긴 것이고, 격돌 전이라도 똑같다.
	{
		TestTrue(TEXT("0.70 reaches the line"), FPaintBarMath::IsPastKoLine(0.70f, Rules.KoLine));
		TestFalse(TEXT("0.69 does not"), FPaintBarMath::IsPastKoLine(0.69f, Rules.KoLine));
		TestTrue(TEXT("danger from 0.65"), FPaintBarMath::IsInDanger(0.65f, Rules));
		TestFalse(TEXT("no danger at 0.64"), FPaintBarMath::IsInDanger(0.64f, Rules));

		const FPaintBarFill Dominant = FPaintBarMath::ComputeFill(0.10f, 0.45f, Rules.ClashCoverage);
		TestFalse(TEXT("dominant before the clash still has a gap"), Dominant.IsClashing());
		TestTrue(TEXT("dominant before the clash is past the line"), FPaintBarMath::IsPastKoLine(Dominant.Right, Rules.KoLine));
	}

	// KO 시계: 버틴 시간이 KoHoldSeconds에 닿으면 KO, 선에서 빠지면 처음부터.
	{
		FPaintKoClock Clock;
		Clock.Advance(true, 2.9f, Rules.KoHoldSeconds);
		TestFalse(TEXT("2.9s is not KO"), Clock.bKnockedOut);
		TestEqual(TEXT("one second left"), Clock.GetSecondsLeft(Rules.KoHoldSeconds), 1);

		Clock.Advance(true, 0.1f, Rules.KoHoldSeconds);
		TestTrue(TEXT("3.0s is KO"), Clock.bKnockedOut);
		TestEqual(TEXT("ring is full"), Clock.GetProgress(Rules.KoHoldSeconds), 1.0f, Tolerance);

		Clock.Advance(false, 0.016f, Rules.KoHoldSeconds);
		TestFalse(TEXT("leaving the line clears KO"), Clock.bKnockedOut);
		TestEqual(TEXT("leaving the line resets the count"), Clock.Held, 0.0f);

		Clock.Advance(true, 1.0f, Rules.KoHoldSeconds);
		TestEqual(TEXT("re-entering counts from zero"), Clock.GetSecondsLeft(Rules.KoHoldSeconds), 2);
	}

	// KO 과장: 이긴 팀이 더 들어오고, 합은 1을 넘지 않는다.
	{
		const FPaintBarFill Pushed = FPaintBarMath::Exaggerate({0.25f, 0.75f}, 0.08f, 0.0f);
		TestEqual(TEXT("winner pushes further"), Pushed.Right, 0.83f, Tolerance);
		TestEqual(TEXT("loser gives way"), Pushed.Left, 0.17f, Tolerance);

		const FPaintBarFill Capped = FPaintBarMath::Exaggerate({0.02f, 0.98f}, 0.08f, 0.0f);
		TestEqual(TEXT("never past the end"), Capped.Right, 1.0f, Tolerance);
		TestEqual(TEXT("never negative"), Capped.Left, 0.0f, Tolerance);
	}

	// 미리보기 곡선은 KO까지 보여줘야 한다.
	{
		const float Period = 20.0f;
		const float Step = 0.05f;
		float Longest = 0.0f;
		float Current = 0.0f;
		for (int32 Sample = 0; Sample * Step < Period; ++Sample)
		{
			const FVector2f Coverage = FPaintBarMath::DemoCoverage(Sample * Step, Period);
			const FPaintBarFill Fill = FPaintBarMath::ComputeFill(Coverage.X, Coverage.Y, Rules.ClashCoverage);
			Current = FPaintBarMath::IsPastKoLine(Fill.Right, Rules.KoLine) ? Current + Step : 0.0f;
			Longest = FMath::Max(Longest, Current);
		}
		TestTrue(TEXT("demo stays past the line longer than the KO hold"), Longest > Rules.KoHoldSeconds);
	}

	// 점멸은 원래 색에서 시작해 반 주기에 가장 빨갛다.
	TestEqual(TEXT("pulse starts at zero"), FPaintBarMath::Pulse(0.0f), 0.0f, Tolerance);
	TestEqual(TEXT("pulse peaks halfway"), FPaintBarMath::Pulse(0.5f), 1.0f, Tolerance);

	return true;
}

#endif
