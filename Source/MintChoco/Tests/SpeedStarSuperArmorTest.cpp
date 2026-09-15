#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"
#include "Items/SpeedStarAbility.h"
#include "Items/SpeedStarProfile.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	USpeedStarProfile* MakeStarProfile(bool bSuperArmor)
	{
		USpeedStarProfile* const Profile = NewObject<USpeedStarProfile>();
		Profile->AbilityClass = UGA_SpeedStar::StaticClass();
		Profile->Duration = 2.0f;
		Profile->bSuperArmor = bSuperArmor;
		return Profile;
	}
}

/**
 * 스피드 스타는 효과 동안 슈퍼아머를 준다. 슈퍼아머 태그(State.Status.SuperArmor) 하나가 스턴·밀어내기
 * 면역과 하이라이트(AUnit::UpdateSuperArmorOutline)를 함께 켜므로, 태그가 효과와 같이 오르내리는지와
 * 스턴이 실제로 막히는지를 본다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpeedStarSuperArmorTest,
	"MintChoco.Items.SpeedStar.SuperArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSpeedStarSuperArmorTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	AUnit* const Unit = World->SpawnActor<AUnit>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator);
	UItemSlotComponent* const Slot = Unit ? Unit->GetItemSlot() : nullptr;
	UAbilitySystemComponent* const AbilitySystem = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
	if (!TestNotNull(TEXT("유닛의 아이템 슬롯"), Slot) || !TestNotNull(TEXT("유닛의 ASC"), AbilitySystem))
	{
		return false;
	}

	// 컨트롤러가 없는 폰이라 빙의 경로가 돌지 않는다. 어빌리티를 굴리려면 이것만 있으면 된다.
	AbilitySystem->InitAbilityActorInfo(Unit, Unit);

	const auto HasStarEffect = [AbilitySystem]()
	{
		return AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_SpeedStar);
	};

	TestTrue(TEXT("스피드 스타는 기본으로 슈퍼아머를 준다"), GetDefault<USpeedStarProfile>()->bSuperArmor);
	TestFalse(TEXT("쓰기 전에는 슈퍼아머가 없다"), Unit->HasSuperArmor());

	// 끈 프로필: 속도만 오르고 슈퍼아머는 없다.
	Slot->GiveItem(MakeStarProfile(/*bSuperArmor=*/false));
	TestTrue(TEXT("슈퍼아머를 끈 스타도 쓸 수 있다"), Slot->TryUseHeldItem());
	TestTrue(TEXT("끈 스타도 효과는 걸린다"), HasStarEffect());
	TestFalse(TEXT("끄면 슈퍼아머가 없다"), Unit->HasSuperArmor());
	MintChocoTest::AdvanceTime(*World, 2.1f);
	TestFalse(TEXT("끈 스타가 끝났다"), HasStarEffect());

	// 켠 프로필: 효과 동안 슈퍼아머.
	Slot->GiveItem(MakeStarProfile(/*bSuperArmor=*/true));
	TestTrue(TEXT("스타를 쓴다"), Slot->TryUseHeldItem());
	TestTrue(TEXT("스타 효과가 걸린다"), HasStarEffect());
	TestTrue(TEXT("스타를 쓰면 슈퍼아머가 켜진다"), Unit->HasSuperArmor());
	TestFalse(TEXT("슈퍼아머 중에는 스턴이 먹지 않는다"), Unit->TryApplyStun());
	TestFalse(TEXT("스턴되지 않았다"), Unit->IsStunned());

	MintChocoTest::AdvanceTime(*World, 1.0f);
	TestTrue(TEXT("효과 도중에도 슈퍼아머가 유지된다"), Unit->HasSuperArmor());

	MintChocoTest::AdvanceTime(*World, 1.1f);
	TestFalse(TEXT("스타 효과가 끝났다"), HasStarEffect());
	TestFalse(TEXT("스타가 끝나면 슈퍼아머도 끝난다"), Unit->HasSuperArmor());
	TestTrue(TEXT("슈퍼아머가 끝나면 다시 스턴된다"), Unit->TryApplyStun());

	return true;
}

#endif
