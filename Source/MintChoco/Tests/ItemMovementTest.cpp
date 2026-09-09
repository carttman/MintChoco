#include "Misc/AutomationTest.h"

#include "Game/UnitMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 부스트 플래그가 서면 대시나 기본 속도와 무관하게 고정 속도로 달린다. */
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
	Movement->SpeedBoostSpeed = 1500.0f;

	TestEqual(TEXT("base speed"), Movement->GetMaxSpeed(), 600.0f);

	Movement->SetWantsToDash(true);
	TestEqual(TEXT("dash multiplies the base"), Movement->GetMaxSpeed(), 1020.0f, 1e-3f);

	int32 BoostChanges = 0;
	Movement->OnSpeedBoostStateChanged.AddLambda([&BoostChanges](bool) { ++BoostChanges; });

	Movement->SetWantsSpeedBoost(true);
	TestTrue(TEXT("boost is remembered"), Movement->WantsSpeedBoost());
	TestEqual(TEXT("boost is a fixed speed, dash ignored"), Movement->GetMaxSpeed(), 1500.0f);
	Movement->SetWantsSpeedBoost(true);
	TestEqual(TEXT("setting the same value again is silent"), BoostChanges, 1);

	Movement->SetWantsToDash(false);
	TestEqual(TEXT("boost survives the dash ending"), Movement->GetMaxSpeed(), 1500.0f);

	Movement->SetWantsSpeedBoost(false);
	TestEqual(TEXT("back to the base"), Movement->GetMaxSpeed(), 600.0f);
	TestEqual(TEXT("one change per transition"), BoostChanges, 2);

	// 서버 경로: 압축 플래그에서 되살린다. 유닛 소유자가 없으면 인정 검사는 항상 통과한다.
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_1);
	TestTrue(TEXT("FLAG_Custom_1 restores the boost"), Movement->WantsSpeedBoost());
	TestFalse(TEXT("FLAG_Custom_1 alone leaves dash off"), Movement->GetMaxSpeed() != 1500.0f);
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_0);
	TestFalse(TEXT("a move without the flag clears the boost"), Movement->WantsSpeedBoost());
	TestEqual(TEXT("FLAG_Custom_0 restores the dash"), Movement->GetMaxSpeed(), 1020.0f, 1e-3f);

	return true;
}

#endif
