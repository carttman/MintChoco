#include "Misc/AutomationTest.h"

#include "Items/SpeedStarAbility.h"
#include "Items/SpeedStarProfile.h"
#include "Paint/PaintBrushProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 코너 뒤 자국 배율: 직진 거리 0이면 둥글고, 거리만큼 차올라 TrailStretch에서 멈추며,
 * 꼬리(반지름 × 배율 + 중심 이동)는 늘 반지름 + 직진 거리 안에 머문다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpeedStarTrailStretchTest,
	"MintChoco.Items.SpeedStar.TrailStretch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSpeedStarTrailStretchTest::RunTest(const FString& Parameters)
{
	UPaintBrushProfile* const Brush = NewObject<UPaintBrushProfile>();
	Brush->BaseRadius = 50.0f;
	Brush->RadiusPerSpeed = 0.0f;
	Brush->MaxStretch = 4.0f;

	const float Radius = Brush->ComputeRadius(1.0f, 0.0f);
	TestEqual(TEXT("radius of unit volume at rest is the base radius"), Radius, 50.0f);

	TestEqual(TEXT("a tail no longer than the radius is a round stamp"), Brush->StretchWithinTail(Radius, Radius), 1.0f);
	TestEqual(TEXT("a very long tail hits the brush ceiling"), Brush->StretchWithinTail(Radius, 100000.0f), 4.0f);

	// CenterShiftPercent의 기본값 50%: 꼬리 = R · (S · 1.5 − 0.5).
	constexpr float Shift = 0.5f;
	float Previous = 1.0f;
	for (float Tail = Radius; Tail <= 6.0f * Radius; Tail += 10.0f)
	{
		const float Stretch = Brush->StretchWithinTail(Radius, Tail);
		TestTrue(TEXT("stretch never shrinks as the allowed tail grows"), Stretch >= Previous);
		const float Reach = Radius * (Stretch * (1.0f + Shift) - Shift);
		TestTrue(FString::Printf(TEXT("tail %.0f: reach %.1f stays within the allowance"), Tail, Reach), Reach <= Tail + 0.01f);
		if (Stretch < Brush->MaxStretch)
		{
			TestTrue(TEXT("below the ceiling the stamp uses the whole allowance"), FMath::IsNearlyEqual(Reach, Tail, 0.01f));
		}
		Previous = Stretch;
	}

	USpeedStarProfile* const Star = NewObject<USpeedStarProfile>();
	Star->TrailDeposit.BrushProfile = Brush;
	Star->TrailDeposit.SplatVolume = 1.0f;
	Star->TrailStretch = 2.5f;

	TestEqual(TEXT("the first mark after a corner is round"), UGA_SpeedStar::StretchForRun(*Star, 0.0f), 1.0f);
	TestEqual(TEXT("a long straight run reaches the profile's stretch, not the brush ceiling"), UGA_SpeedStar::StretchForRun(*Star, 10000.0f), 2.5f);
	const float Mid = UGA_SpeedStar::StretchForRun(*Star, Radius);
	TestTrue(TEXT("one radius of straight run is part way there"), Mid > 1.0f && Mid < 2.5f);

	Star->TrailDeposit.BrushProfile = nullptr;
	TestEqual(TEXT("without a brush the trail stays round"), UGA_SpeedStar::StretchForRun(*Star, 10000.0f), 1.0f);

	return true;
}

#endif
