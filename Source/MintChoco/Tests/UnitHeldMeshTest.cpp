#include "Misc/AutomationTest.h"

#include "Engine/StaticMesh.h"

#include "Game/Unit.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 조준 중 손에 든 메시를 언제 보이는가.
 *
 * 조준 중인 것만으로는 부족하다: 들 메시를 지정하지 않은 아이템(조준형이 늘어날 때)은 손이
 * 비어 있어야 한다. 반대로 메시가 있어도 조준 중이 아니면 들지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitHeldMeshTest,
	"MintChoco.Game.Unit.HeldMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnitHeldMeshTest::RunTest(const FString& Parameters)
{
	const UStaticMesh* const Mesh = NewObject<UStaticMesh>();

	TestTrue(TEXT("조준 중이고 메시가 있으면 든다"), AUnit::ShouldShowHeld(true, Mesh));
	TestFalse(TEXT("메시를 지정하지 않은 아이템은 손이 빈다"), AUnit::ShouldShowHeld(true, nullptr));
	TestFalse(TEXT("조준 중이 아니면 들지 않는다"), AUnit::ShouldShowHeld(false, Mesh));
	TestFalse(TEXT("둘 다 아니면 당연히 들지 않는다"), AUnit::ShouldShowHeld(false, nullptr));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
