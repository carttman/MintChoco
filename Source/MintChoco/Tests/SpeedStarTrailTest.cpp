#include "Misc/AutomationTest.h"

#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

#include "Items/SpeedStarAbility.h"
#include "Items/SpeedStarProfile.h"
#include "Paint/PaintBrushProfile.h"
#include "Tests/TestUnitCharacter.h"
#include "Tests/TestWorld.h"

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

/**
 * 자국을 찍을 바닥을 얼마나 멀리까지 찾는가.
 *
 * 예전에는 발밑 60cm 고정이라 살짝만 떠도(점프, 히어로 랜딩) 트레이스가 아무것도 맞히지 못해
 * 자국이 끊겼다. 공중에서도 지나간 자리 아래가 칠해져야 한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpeedStarGroundReachTest,
	"MintChoco.Items.SpeedStar.GroundReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSpeedStarGroundReachTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	// 윗면이 Z=0인 바닥.
	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	ATestUnitCharacter* const Character = World->SpawnActor<ATestUnitCharacter>(FVector(0.0f, 0.0f, 500.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("테스트 캐릭터"), Character))
	{
		return false;
	}

	USpeedStarProfile* const Profile = NewObject<USpeedStarProfile>();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FHitResult Hit;

	// 땅에 붙어 달릴 때. 예전 거리(60cm)로도 찾던 경우이고, 늘려도 같은 바닥을 찾아야 한다.
	const FVector OnGround(0.0f, 0.0f, HalfHeight);
	if (TestTrue(TEXT("땅에 붙어 달리면 바닥을 찾는다"), UGA_SpeedStar::FindTrailGround(*Character, *Profile, OnGround, Hit)))
	{
		TestTrue(TEXT("찾은 바닥은 발밑이다"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 0.0f, 1.0f));
	}

	// 이게 이 테스트의 핵심: 발밑 500cm 떠 있어도 아래 바닥을 찾는다.
	const FVector InAir(0.0f, 0.0f, HalfHeight + 500.0f);
	if (TestTrue(TEXT("공중에 떠 있어도 아래 바닥을 찾는다"), UGA_SpeedStar::FindTrailGround(*Character, *Profile, InAir, Hit)))
	{
		TestTrue(TEXT("공중에서도 같은 바닥이다"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 0.0f, 1.0f));
	}

	// 다만 끝없이 칠하지는 않는다. 사거리가 곧 "얼마나 높은 데서 칠할 수 있는가"다.
	const FVector TooHigh(0.0f, 0.0f, HalfHeight + Profile->MarkGroundReach + 100.0f);
	TestFalse(TEXT("사거리 밖에서는 바닥을 찾지 않는다"), UGA_SpeedStar::FindTrailGround(*Character, *Profile, TooHigh, Hit));

	// 사거리를 0으로 두면 발밑만 본다. 옛 동작으로 되돌리고 싶을 때의 탈출구다.
	Profile->MarkGroundReach = 0.0f;
	TestFalse(TEXT("사거리가 0이면 공중에서는 찾지 않는다"), UGA_SpeedStar::FindTrailGround(*Character, *Profile, InAir, Hit));

	return true;
}

#endif
