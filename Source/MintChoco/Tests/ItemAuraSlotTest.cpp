#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "Items/ItemSlotComponent.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 오라가 상태 태그를 따라 캐릭터 메시의 오버레이로 오르내리는지. 겹쳤을 때의 순서는
 * MintChoco.Items.Aura.Stack이 보므로 여기서는 배선만 본다.
 *
 * 연출이 읽는 프로필은 슬롯에 든 인스턴스가 아니라 설정 목록의 에셋이다: 상태 태그만 복제되므로
 * 구경꾼도 같은 연출을 찾으려면 태그로 되짚는 수밖에 없다(UItemSlotComponent::HandleTagChanged).
 * 그래서 출하 에셋의 오라를 잠깐 빌리고 되돌린다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemAuraSlotTest,
	"MintChoco.Items.Aura.Slot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemAuraSlotTest::RunTest(const FString& Parameters)
{
	UItemProfile* const Star = UItemSettings::Get().FindItemByStateTag(ItemTags::State_Item_SpeedStar);
	if (!TestNotNull(TEXT("설정 목록의 스피드 스타"), Star))
	{
		return false;
	}

	UMaterialInterface* const SavedAura = Star->AuraMaterial;
	ON_SCOPE_EXIT { Star->AuraMaterial = SavedAura; };

	UMaterial* const Aura = NewObject<UMaterial>();
	Star->AuraMaterial = Aura;

	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	AUnit* const Unit = World->SpawnActor<AUnit>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator);
	UItemSlotComponent* const Slot = Unit ? Unit->GetItemSlot() : nullptr;
	UAbilitySystemComponent* const AbilitySystem = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
	USkeletalMeshComponent* const Mesh = Unit ? Unit->GetMesh() : nullptr;
	if (!TestNotNull(TEXT("유닛의 아이템 슬롯"), Slot) || !TestNotNull(TEXT("유닛의 ASC"), AbilitySystem)
		|| !TestNotNull(TEXT("유닛의 메시"), Mesh))
	{
		return false;
	}

	// 테스트 월드에는 게임모드가 없어 bBegunPlay가 서지 않고, 그래서 스폰된 액터도 컴포넌트도
	// BeginPlay를 받지 못한다. 오라는 슬롯이 BeginPlay에서 거는 태그 이벤트를 타므로 직접 돌린다.
	Unit->DispatchBeginPlay();

	// 컨트롤러가 없는 폰이라 빙의 경로가 돌지 않는다. 어빌리티를 굴리려면 이것만 있으면 된다.
	AbilitySystem->InitAbilityActorInfo(Unit, Unit);

	TestNull(TEXT("아무것도 안 썼으면 오라가 없다"), Mesh->GetOverlayMaterial());

	Slot->GiveItem(Star);
	TestTrue(TEXT("스타를 쓴다"), Slot->TryUseHeldItem());
	TestTrue(TEXT("스타 효과가 걸린다"), AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_SpeedStar));
	TestEqual(TEXT("효과가 도는 동안 그 아이템의 오라가 씌워진다"),
		Mesh->GetOverlayMaterial(), static_cast<UMaterialInterface*>(Aura));

	MintChocoTest::AdvanceTime(*World, Star->Duration + 0.1f);
	TestFalse(TEXT("스타 효과가 끝났다"), AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_SpeedStar));
	TestNull(TEXT("효과가 끝나면 오라가 걷힌다"), Mesh->GetOverlayMaterial());

	return true;
}

#endif
