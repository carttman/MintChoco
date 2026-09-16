#include "Misc/AutomationTest.h"

#include "Game/UnitMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 부스트 플래그가 서면 기본 속도에 배율이 곱해지고, 대시 중이면 대시 배율도 함께 곱해진다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemMovementSpeedBoostTest,
	"MintChoco.Items.Movement.SpeedBoost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemMovementSpeedBoostTest::RunTest(const FString& Parameters)
{
	UUnitMovementComponent* const Movement = NewObject<UUnitMovementComponent>();
	Movement->MovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 600.0f;
	Movement->DashSpeedMultiplier = 1.7f;
	Movement->SpeedBoostMultiplier = 1.5f;

	TestEqual(TEXT("base speed"), Movement->GetMaxSpeed(), 600.0f);

	Movement->SetWantsToDash(true);
	TestEqual(TEXT("dash multiplies the base"), Movement->GetMaxSpeed(), 1020.0f, 1e-3f);

	int32 BoostChanges = 0;
	Movement->OnSpeedBoostStateChanged.AddLambda([&BoostChanges](bool) { ++BoostChanges; });

	Movement->SetWantsSpeedBoost(true);
	TestTrue(TEXT("boost is remembered"), Movement->WantsSpeedBoost());
	// 600 × 1.5(바닥) × 1.5(부스트) × 1.7(대시). 부스트 중에는 바닥 배율이 내 색으로 굳으므로
	// 상대 진영 한복판에서도 같은 속도가 나온다 - 그것이 스피드 스타가 보장하는 최소 속도다.
	TestEqual(TEXT("boost and dash multiply together"), Movement->GetMaxSpeed(), 2295.0f, 1e-3f);
	TestTrue(TEXT("boosted dash is faster than plain dash"), Movement->GetMaxSpeed() > 1020.0f);
	Movement->SetWantsSpeedBoost(true);
	TestEqual(TEXT("setting the same value again is silent"), BoostChanges, 1);

	Movement->SetWantsToDash(false);
	TestEqual(TEXT("boost survives the dash ending"), Movement->GetMaxSpeed(), 1350.0f, 1e-3f);

	Movement->SetWantsSpeedBoost(false);
	TestEqual(TEXT("back to the base"), Movement->GetMaxSpeed(), 600.0f);
	TestEqual(TEXT("one change per transition"), BoostChanges, 2);

	// 서버 경로: 압축 플래그에서 되살린다. 유닛 소유자가 없으면 인정 검사는 항상 통과한다.
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_1);
	TestTrue(TEXT("FLAG_Custom_1 restores the boost"), Movement->WantsSpeedBoost());
	TestEqual(TEXT("FLAG_Custom_1 alone: boost without dash"), Movement->GetMaxSpeed(), 1350.0f, 1e-3f);
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_0);
	TestFalse(TEXT("a move without the flag clears the boost"), Movement->WantsSpeedBoost());
	TestEqual(TEXT("FLAG_Custom_0 restores the dash"), Movement->GetMaxSpeed(), 1020.0f, 1e-3f);

	return true;
}

/**
 * 잉크 회복 배율은 속도와 같은 두 배율을 쓰되 부스트만 뺀다. 별을 먹었다고 잉크가 1.5배로
 * 차지는 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemMovementInkRefillMultiplierTest,
	"MintChoco.Items.Movement.InkRefillMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemMovementInkRefillMultiplierTest::RunTest(const FString& Parameters)
{
	UUnitMovementComponent* const Movement = NewObject<UUnitMovementComponent>();
	Movement->MovementMode = MOVE_Walking;
	Movement->DashSpeedMultiplier = 2.0f;
	Movement->SpeedBoostMultiplier = 1.5f;
	Movement->OwnFloorMultiplier = 1.5f;

	// 주인이 유닛이 아니면 바닥 색을 견줄 상대가 없다. 미도색과 같은 1이다.
	TestEqual(TEXT("걷기, 바닥 없음"), Movement->GetInkRefillMultiplier(), 1.0f, 1e-3f);

	Movement->SetWantsToDash(true);
	TestEqual(TEXT("보드를 타면 두 배"), Movement->GetInkRefillMultiplier(), 2.0f, 1e-3f);

	// 부스트는 바닥을 내 색으로 굳히므로 바닥 배율은 오르지만, 부스트 배율 자체는 안 곱해진다.
	Movement->SetWantsSpeedBoost(true);
	TestEqual(TEXT("부스트는 잉크 배율에 안 곱해진다"), Movement->GetInkRefillMultiplier(), 3.0f, 1e-3f);

	return true;
}

#endif
