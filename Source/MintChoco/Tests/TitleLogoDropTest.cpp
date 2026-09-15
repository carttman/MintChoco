#include "Misc/AutomationTest.h"

#include "UI/TitleWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 높이 곡선: 1에서 시작해 0에서 끝나고, 튈 때마다 낮아지며, 끊기거나 착지점 아래로 내려가지 않는다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTitleLogoDropCurveTest,
	"MintChoco.UI.TitleLogoDrop.Curve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTitleLogoDropCurveTest::RunTest(const FString& Parameters)
{
	const float Restitution = 0.4f;
	const int32 Bounces = 2;

	TestEqual(TEXT("starts at the top"), FTitleLogoDrop::EvaluateHeight(0.0f, Restitution, Bounces), 1.0f);
	TestEqual(TEXT("ends on the ground"), FTitleLogoDrop::EvaluateHeight(1.0f, Restitution, Bounces), 0.0f, 1e-4f);
	TestEqual(TEXT("alpha below 0 clamps"), FTitleLogoDrop::EvaluateHeight(-1.0f, Restitution, Bounces), 1.0f);
	TestEqual(TEXT("alpha above 1 clamps"), FTitleLogoDrop::EvaluateHeight(2.0f, Restitution, Bounces), 0.0f, 1e-4f);

	// 전체 시간 단위는 1 + 2e + 2e² = 2.12. 첫 착지는 그 1/2.12 지점이다.
	const float Total = 1.0f + 2.0f * Restitution + 2.0f * Restitution * Restitution;
	TestEqual(TEXT("first landing"), FTitleLogoDrop::EvaluateHeight(1.0f / Total, Restitution, Bounces), 0.0f, 1e-3f);
	TestTrue(TEXT("still falling just before"), FTitleLogoDrop::EvaluateHeight(0.95f / Total, Restitution, Bounces) > 0.05f);

	const int32 Samples = 4000;
	float Previous = 1.0f;
	float MaxStep = 0.0f;
	float MinHeight = 1.0f;
	float MaxHeight = 0.0f;
	bool bRising = false;
	TArray<float> Peaks;
	for (int32 Index = 1; Index <= Samples; ++Index)
	{
		const float Height = FTitleLogoDrop::EvaluateHeight(static_cast<float>(Index) / Samples, Restitution, Bounces);
		MaxStep = FMath::Max(MaxStep, FMath::Abs(Height - Previous));
		MinHeight = FMath::Min(MinHeight, Height);
		MaxHeight = FMath::Max(MaxHeight, Height);
		if (Height > Previous)
		{
			bRising = true;
		}
		else if (bRising && Height < Previous)
		{
			Peaks.Add(Previous);
			bRising = false;
		}
		Previous = Height;
	}

	TestTrue(TEXT("never below the resting spot"), MinHeight >= 0.0f);
	TestTrue(TEXT("never above the start"), MaxHeight <= 1.0f);
	TestTrue(*FString::Printf(TEXT("no jumps (largest step %f)"), MaxStep), MaxStep < 0.01f);
	if (TestEqual(TEXT("bounces as many times as asked"), Peaks.Num(), Bounces))
	{
		// n번째 꼭대기는 e^(2n): 0.16, 0.0256.
		TestEqual(TEXT("first bounce height"), Peaks[0], FMath::Pow(Restitution, 2.0f), 0.005f);
		TestEqual(TEXT("second bounce height"), Peaks[1], FMath::Pow(Restitution, 4.0f), 0.005f);
		TestTrue(TEXT("each bounce is lower"), Peaks[1] < Peaks[0]);
	}

	// 튀지 않으면 끝에서 정확히 닿고, 그때까지 줄곧 내려오기만 한다.
	Previous = 1.0f;
	bool bMonotonic = true;
	for (int32 Index = 1; Index <= Samples; ++Index)
	{
		const float Height = FTitleLogoDrop::EvaluateHeight(static_cast<float>(Index) / Samples, Restitution, 0);
		bMonotonic &= Height <= Previous;
		Previous = Height;
	}
	TestTrue(TEXT("no bounces: only falls"), bMonotonic);
	TestTrue(TEXT("no bounces: lands at the very end"), FTitleLogoDrop::EvaluateHeight(0.99f, Restitution, 0) > 0.0f);

	// 반발이 0이면 횟수와 무관하게 튀지 않는다.
	for (const float Alpha : {0.1f, 0.5f, 0.9f})
	{
		TestEqual(*FString::Printf(TEXT("zero restitution at %.1f"), Alpha),
			FTitleLogoDrop::EvaluateHeight(Alpha, 0.0f, 3), FTitleLogoDrop::EvaluateHeight(Alpha, 0.4f, 0), 1e-5f);
	}

	return true;
}

/** 시계: 가림막 아래에서는 기다리고, 열리면 잠깐 뒤 떨어져, 멈춤에도 건너뛰지 않고 착지한다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTitleLogoDropTimelineTest,
	"MintChoco.UI.TitleLogoDrop.Timeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTitleLogoDropTimelineTest::RunTest(const FString& Parameters)
{
	using EPhase = FTitleLogoDrop::EPhase;

	FTitleLogoDrop Drop;
	Drop.Duration = 1.0f;
	Drop.Delay = 0.2f;
	Drop.Restitution = 0.4f;
	Drop.Bounces = 2;
	Drop.Restart();

	// 가림막이 덮고 있는 동안에는 몇 초가 지나도 떨어지지 않고 보이지도 않는다.
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Drop.Tick(0.03f, false);
	}
	TestTrue(TEXT("waits under the cover"), Drop.Phase == EPhase::WaitingForScreen);
	TestFalse(TEXT("hidden under the cover"), Drop.IsLogoVisible());
	TestEqual(TEXT("held at the top"), Drop.GetHeight(), 1.0f);

	// 화면이 열리면 Delay만큼 더 기다린다.
	TestTrue(TEXT("opening the screen changes phase"), Drop.Tick(0.03f, true));
	TestTrue(TEXT("delaying"), Drop.Phase == EPhase::Delaying);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Drop.Tick(0.03f, true);
	}
	TestTrue(TEXT("still delaying at 0.18 s"), Drop.Phase == EPhase::Delaying);
	TestFalse(TEXT("still hidden while delaying"), Drop.IsLogoVisible());

	TestTrue(TEXT("delay over changes phase"), Drop.Tick(0.03f, true));
	TestTrue(TEXT("dropping at 0.21 s"), Drop.Phase == EPhase::Dropping);
	TestTrue(TEXT("visible once dropping"), Drop.IsLogoVisible());
	TestTrue(TEXT("starts near the top"), Drop.GetHeight() > 0.9f);

	// 긴 멈춤 한 틱이 연출을 건너뛰지 않는다.
	Drop.Tick(5.0f, true);
	TestTrue(TEXT("a hitch does not skip the drop"), Drop.Phase == EPhase::Dropping);

	// 가림막이 다시 덮어도 떨어지던 것은 끝까지 간다.
	for (int32 Index = 0; Index < 100 && Drop.Phase != EPhase::Landed; ++Index)
	{
		Drop.Tick(0.03f, false);
	}
	TestTrue(TEXT("lands"), Drop.Phase == EPhase::Landed);
	TestEqual(TEXT("rests on the spot"), Drop.GetHeight(), 0.0f);
	TestTrue(TEXT("stays visible"), Drop.IsLogoVisible());

	Drop.Restart();
	TestTrue(TEXT("restart waits again"), Drop.Phase == EPhase::WaitingForScreen);
	TestEqual(TEXT("restart holds at the top"), Drop.GetHeight(), 1.0f);

	return true;
}

#endif
