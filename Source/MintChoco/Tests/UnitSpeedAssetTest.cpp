#include "Misc/AutomationTest.h"

#include "Game/Unit.h"
#include "Game/UnitMovementComponent.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* const UnitClassPath = TEXT("/Game/Blueprints/Game/BP_Unit.BP_Unit_C");
}

/**
 * 출하되는 유닛에서 스피드 스타가 실제로 빨라지는지 지킨다. 예전 부스트는 고정 속도(1500)였고,
 * BP_Unit의 기본 속도가 1000으로 오르자 대시(1700)가 부스트보다 빨라져 대시 중에 아이템을 쓰면
 * 오히려 느려졌다. 코드 테스트는 기본값만 보므로 이런 조합은 에셋에서만 잡힌다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitSpeedAssetTest,
	"MintChoco.Game.Movement.SpeedAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnitSpeedAssetTest::RunTest(const FString& Parameters)
{
	UClass* const UnitClass = LoadClass<AUnit>(nullptr, UnitClassPath);
	if (!TestNotNull(TEXT("BP_Unit loads"), UnitClass))
	{
		return false;
	}

	const AUnit* const Defaults = UnitClass->GetDefaultObject<AUnit>();
	const UUnitMovementComponent* const Movement = Defaults ? Cast<UUnitMovementComponent>(Defaults->GetCharacterMovement()) : nullptr;
	if (!TestNotNull(TEXT("BP_Unit has a unit movement component"), Movement))
	{
		return false;
	}

	const float Walk = Movement->MaxWalkSpeed;
	const float Dash = Walk * Movement->DashSpeedMultiplier;
	const float BoostedWalk = Walk * Movement->SpeedBoostMultiplier;
	const float BoostedDash = Dash * Movement->SpeedBoostMultiplier;
	AddInfo(FString::Printf(TEXT("BP_Unit: walk %.0f, dash %.0f (x%.2f), boosted walk %.0f, boosted dash %.0f (boost x%.2f), acceleration %.0f"),
		Walk, Dash, Movement->DashSpeedMultiplier, BoostedWalk, BoostedDash, Movement->SpeedBoostMultiplier, Movement->MaxAcceleration));

	TestTrue(TEXT("speed boost makes walking faster"), BoostedWalk > Walk);
	TestTrue(TEXT("speed boost makes dashing faster"), BoostedDash > Dash);

	return true;
}

#endif
