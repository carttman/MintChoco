#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"
#include "Tests/TestItemAbility.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UTestItemProfile* MakeRecoveryProfile(TSubclassOf<UItemAbility> Ability, float Duration)
	{
		UTestItemProfile* const Profile = NewObject<UTestItemProfile>();
		Profile->AbilityClass = Ability;
		Profile->Duration = Duration;
		return Profile;
	}

	/** 켜져 있는 테스트 어빌리티. 없으면 nullptr. */
	UTestRecoveryItemAbility* FindActiveRecovery(UAbilitySystemComponent& AbilitySystem)
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			for (UGameplayAbility* const Instance : Spec.GetAbilityInstances())
			{
				UTestRecoveryItemAbility* const Ability = Cast<UTestRecoveryItemAbility>(Instance);
				if (Ability && Ability->IsActive())
				{
					return Ability;
				}
			}
		}
		return nullptr;
	}

}

/**
 * 마무리(recovery): 효과가 끝나면 상태 태그가 내려가 무기가 풀리고, 어빌리티는 마무리 동안만 더 산다.
 * 마무리는 발사·다른 아이템 사용으로 끊기고, 강제 종료와 마무리 없는 아이템은 예전처럼 바로 끝난다.
 *
 * 스위트 스피너의 끝 동작 중에 총과 아이템이 바로 나가는 것이 이 흐름에 걸려 있다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemRecoveryFlowTest,
	"MintChoco.Items.Recovery.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemRecoveryFlowTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		UTestRecoveryItemAbility::EffectSeconds = 1.0f;
		UTestRecoveryItemAbility::RecoverySeconds = 0.5f;
		MintChocoTest::DestroyWorld(World);
	};

	AUnit* const Unit = World->SpawnActor<AUnit>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator);
	UItemSlotComponent* const Slot = Unit ? Unit->GetItemSlot() : nullptr;
	UAbilitySystemComponent* const AbilitySystem = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
	if (!TestNotNull(TEXT("유닛의 아이템 슬롯"), Slot) || !TestNotNull(TEXT("유닛의 ASC"), AbilitySystem))
	{
		return false;
	}

	// 컨트롤러가 없는 폰이라 빙의 경로가 돌지 않는다. 어빌리티를 굴리려면 이것만 있으면 된다.
	AbilitySystem->InitAbilityActorInfo(Unit, Unit);

	// 무기는 이 태그로 막힌다(UPaintWeaponComponent::TriggerBlockedTags). 태그가 곧 "쏠 수 있는가"다.
	const auto HasStateTag = [AbilitySystem]()
	{
		return AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_SweetSpinner);
	};

	// 1. 끝까지 두면: 효과 → 마무리 → 끝.
	UTestRecoveryItemAbility::ResetCounters();
	Slot->GiveItem(MakeRecoveryProfile(UTestRecoveryItemAbility::StaticClass(), 2.0f));
	TestTrue(TEXT("아이템을 쓰면 켜진다"), Slot->TryUseHeldItem());

	UTestRecoveryItemAbility* Ability = FindActiveRecovery(*AbilitySystem);
	if (!TestNotNull(TEXT("켜진 어빌리티"), Ability))
	{
		return false;
	}
	TestTrue(TEXT("효과 중에는 태그가 있다"), HasStateTag());
	TestFalse(TEXT("효과 중은 마무리가 아니다"), Ability->IsRecovering());

	// 효과 중에는 발사로 끊기지 않는다: 끊는 것은 마무리뿐이다.
	MintChocoTest::AdvanceTime(*World,0.5f);
	Slot->InterruptItemRecovery();
	TestTrue(TEXT("효과 중의 발사는 효과를 끊지 않는다"), FindActiveRecovery(*AbilitySystem) == Ability);
	TestTrue(TEXT("효과 중의 발사 뒤에도 태그가 있다"), HasStateTag());

	MintChocoTest::AdvanceTime(*World,0.6f);
	TestFalse(TEXT("효과 시간이 지나면 태그가 내려간다"), HasStateTag());
	TestTrue(TEXT("어빌리티는 마무리로 산다"), FindActiveRecovery(*AbilitySystem) == Ability && Ability->IsRecovering());
	TestEqual(TEXT("마무리 시작은 한 번"), UTestRecoveryItemAbility::RecoveryStarts, 1);
	TestEqual(TEXT("아직 끝나지 않았다"), UTestRecoveryItemAbility::Ends, 0);

	MintChocoTest::AdvanceTime(*World,0.2f);
	TestNotNull(TEXT("마무리 도중에는 살아 있다"), FindActiveRecovery(*AbilitySystem));

	MintChocoTest::AdvanceTime(*World,0.4f);
	TestNull(TEXT("마무리가 지나면 끝난다"), FindActiveRecovery(*AbilitySystem));
	TestEqual(TEXT("끝은 한 번"), UTestRecoveryItemAbility::Ends, 1);

	// 2. 마무리 중 발사: 그 자리에서 끝난다.
	UTestRecoveryItemAbility::ResetCounters();
	Slot->GiveItem(MakeRecoveryProfile(UTestRecoveryItemAbility::StaticClass(), 2.0f));
	TestTrue(TEXT("다시 쓸 수 있다"), Slot->TryUseHeldItem());
	MintChocoTest::AdvanceTime(*World,1.1f);
	Ability = FindActiveRecovery(*AbilitySystem);
	TestTrue(TEXT("발사 전에는 마무리 중"), Ability && Ability->IsRecovering());
	Slot->InterruptItemRecovery();
	TestNull(TEXT("발사가 마무리를 끊는다"), FindActiveRecovery(*AbilitySystem));
	TestEqual(TEXT("끊겨도 끝은 한 번"), UTestRecoveryItemAbility::Ends, 1);
	MintChocoTest::AdvanceTime(*World,1.0f);
	TestEqual(TEXT("끊긴 뒤 마무리 타이머가 다시 끝내지 않는다"), UTestRecoveryItemAbility::Ends, 1);

	// 3. 마무리 중 다른 아이템 사용: 켜지는 것만으로 끝난다.
	UTestRecoveryItemAbility::ResetCounters();
	Slot->GiveItem(MakeRecoveryProfile(UTestRecoveryItemAbility::StaticClass(), 2.0f));
	TestTrue(TEXT("세 번째 사용"), Slot->TryUseHeldItem());
	MintChocoTest::AdvanceTime(*World,1.1f);
	TestNotNull(TEXT("다른 아이템을 쓰기 전에는 마무리 중"), FindActiveRecovery(*AbilitySystem));
	Slot->GiveItem(MakeRecoveryProfile(UTestInstantItemAbility::StaticClass(), 0.0f));
	TestTrue(TEXT("마무리 중에 다른 아이템을 쓸 수 있다"), Slot->TryUseHeldItem());
	TestNull(TEXT("다른 아이템이 마무리를 끊는다"), FindActiveRecovery(*AbilitySystem));
	TestEqual(TEXT("다른 아이템에 끊겨도 끝은 한 번"), UTestRecoveryItemAbility::Ends, 1);

	// 4. 강제 종료(FinishItem): 마무리 없이 끝난다.
	UTestRecoveryItemAbility::ResetCounters();
	Slot->GiveItem(MakeRecoveryProfile(UTestRecoveryItemAbility::StaticClass(), 2.0f));
	TestTrue(TEXT("네 번째 사용"), Slot->TryUseHeldItem());
	MintChocoTest::AdvanceTime(*World,0.5f);
	Ability = FindActiveRecovery(*AbilitySystem);
	if (TestNotNull(TEXT("강제 종료 전에는 켜져 있다"), Ability))
	{
		Ability->FinishItem();
	}
	TestNull(TEXT("강제 종료는 바로 끝난다"), FindActiveRecovery(*AbilitySystem));
	TestFalse(TEXT("강제 종료 뒤 태그가 없다"), HasStateTag());
	TestEqual(TEXT("강제 종료는 마무리를 거치지 않는다"), UTestRecoveryItemAbility::RecoveryStarts, 0);
	TestEqual(TEXT("강제 종료도 끝은 한 번"), UTestRecoveryItemAbility::Ends, 1);

	// 5. 마무리가 없는 아이템: 예전처럼 효과와 함께 끝난다.
	UTestRecoveryItemAbility::ResetCounters();
	UTestRecoveryItemAbility::RecoverySeconds = 0.0f;
	Slot->GiveItem(MakeRecoveryProfile(UTestRecoveryItemAbility::StaticClass(), 2.0f));
	TestTrue(TEXT("다섯 번째 사용"), Slot->TryUseHeldItem());
	MintChocoTest::AdvanceTime(*World,1.1f);
	TestNull(TEXT("마무리가 없으면 효과와 함께 끝난다"), FindActiveRecovery(*AbilitySystem));
	TestEqual(TEXT("마무리가 없으면 시작도 없다"), UTestRecoveryItemAbility::RecoveryStarts, 0);
	TestEqual(TEXT("마무리가 없어도 끝은 한 번"), UTestRecoveryItemAbility::Ends, 1);

	return true;
}

#endif
