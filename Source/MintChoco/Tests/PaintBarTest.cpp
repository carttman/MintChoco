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

	// KO 판정선: 양 끝에서 KoLine 만큼 들어온 고정 자리. 상대 게이지가 선에 닿으면 넘긴 것이고, 격돌 전이라도 똑같다.
	{
		TestTrue(TEXT("0.70 reaches the line"), FPaintBarMath::IsPastKoLine(0.70f, Rules.KoLine));
		TestFalse(TEXT("0.69 does not"), FPaintBarMath::IsPastKoLine(0.69f, Rules.KoLine));
		TestTrue(TEXT("danger from 0.65"), FPaintBarMath::IsInDanger(0.65f, Rules.KoLine, Rules.DangerMargin));
		TestFalse(TEXT("no danger at 0.64"), FPaintBarMath::IsInDanger(0.64f, Rules.KoLine, Rules.DangerMargin));

		// 격돌 전: 0.45 / 0.6 = 0.75 로 선(0.7)을 넘긴다. 합이 아직 60 % 에 못 미쳐도 몰아붙일 수 있다.
		const FPaintBarFill Dominant = FPaintBarMath::ComputeFill(0.10f, 0.45f, Rules.ClashCoverage);
		TestFalse(TEXT("dominant before the clash still has a gap"), Dominant.IsClashing());
		TestTrue(TEXT("dominant before the clash is past the line"), FPaintBarMath::IsPastKoLine(Dominant.Right, Rules.KoLine));

		// 격돌 뒤: 몫이 기준이라 절대 점유율은 상관없다. 0.24 : 0.56 은 0.7 이고, 0.25 : 0.55 는 아니다.
		const FPaintBarFill Share = FPaintBarMath::ComputeFill(0.24f, 0.56f, Rules.ClashCoverage);
		TestTrue(TEXT("70 % of the bar reaches the line"), FPaintBarMath::IsPastKoLine(Share.Right, Rules.KoLine));
		const FPaintBarFill Short = FPaintBarMath::ComputeFill(0.25f, 0.55f, Rules.ClashCoverage);
		TestFalse(TEXT("69 % of the bar does not"), FPaintBarMath::IsPastKoLine(Short.Right, Rules.KoLine));
	}

	// 로컬 KO 시계(GameState 가 없을 때): 버틴 시간이 유지 시간에 닿으면 KO, 선에서 빠지면 처음부터.
	{
		const float Hold = 3.0f;
		FPaintKoClock Clock;
		Clock.Advance(true, 2.9f, Hold);
		TestFalse(TEXT("2.9s is not KO"), Clock.bKnockedOut);
		TestEqual(TEXT("one second left"), Clock.GetSecondsLeft(Hold), 1);

		Clock.Advance(true, 0.1f, Hold);
		TestTrue(TEXT("3.0s is KO"), Clock.bKnockedOut);
		TestEqual(TEXT("ring is full"), Clock.GetProgress(Hold), 1.0f, Tolerance);

		Clock.Advance(false, 0.016f, Hold);
		TestFalse(TEXT("leaving the line clears KO"), Clock.bKnockedOut);
		TestEqual(TEXT("leaving the line resets the count"), Clock.Held, 0.0f);

		Clock.Advance(true, 1.0f, Hold);
		TestEqual(TEXT("re-entering counts from zero"), Clock.GetSecondsLeft(Hold), 2);
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
			TestTrue(TEXT("demo never paints more than the world"), Coverage.X + Coverage.Y <= 1.0f + Tolerance);
			const FPaintBarFill Fill = FPaintBarMath::ComputeFill(Coverage.X, Coverage.Y, Rules.ClashCoverage);
			Current = FPaintBarMath::IsPastKoLine(Fill.Right, Rules.KoLine) ? Current + Step : 0.0f;
			Longest = FMath::Max(Longest, Current);
		}
		TestTrue(TEXT("demo stays past the line longer than the KO hold"), Longest > Rules.PreviewKoHoldSeconds);
	}

	// 점멸은 원래 색에서 시작해 반 주기에 가장 빨갛다.
	TestEqual(TEXT("pulse starts at zero"), FPaintBarMath::Pulse(0.0f), 0.0f, Tolerance);
	TestEqual(TEXT("pulse peaks halfway"), FPaintBarMath::Pulse(0.5f), 1.0f, Tolerance);

	return true;
}

#endif
