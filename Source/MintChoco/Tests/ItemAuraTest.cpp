#include "Misc/AutomationTest.h"

#include "Materials/Material.h"

#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 오라 스택: 오버레이는 한 칸이므로 마지막에 켠 것이 보이고, 그것이 꺼지면 앞의 것이 돌아오며,
 * 전부 꺼지면 아무것도 남지 않는다(효과가 끝났는데 오라만 남는 일이 없다). 오라를 정하지 않은
 * 아이템은 남의 오라를 덮지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemAuraStackTest,
	"MintChoco.Items.Aura.Stack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemAuraStackTest::RunTest(const FString& Parameters)
{
	UMaterial* const Speed = NewObject<UMaterial>();
	UMaterial* const Ammo = NewObject<UMaterial>();

	FItemAuraStack Stack;
	TestNull(TEXT("아무것도 안 켰으면 오라가 없다"), Stack.Top());

	Stack.Push(ItemTags::State_Item_SpeedStar, Speed);
	TestEqual(TEXT("하나 켜면 그것이 보인다"), Stack.Top(), static_cast<UMaterialInterface*>(Speed));

	// 겹침: 무한 탄환이 도는 중에 스피드 스타를 써도 태그는 둘 다 산다.
	Stack.Push(ItemTags::State_Item_InfiniteAmmo, Ammo);
	TestEqual(TEXT("겹치면 마지막에 켠 것이 이긴다"), Stack.Top(), static_cast<UMaterialInterface*>(Ammo));

	Stack.Push(ItemTags::State_Item_SweetSpinner, nullptr);
	TestEqual(TEXT("오라 없는 아이템은 남의 오라를 덮지 않는다"), Stack.Top(), static_cast<UMaterialInterface*>(Ammo));

	Stack.Pop(ItemTags::State_Item_InfiniteAmmo);
	TestEqual(TEXT("마지막 것이 꺼지면 앞의 것이 돌아온다"), Stack.Top(), static_cast<UMaterialInterface*>(Speed));

	Stack.Pop(ItemTags::State_Item_HeroLanding);
	TestEqual(TEXT("켠 적 없는 태그를 꺼도 아무 일이 없다"), Stack.Top(), static_cast<UMaterialInterface*>(Speed));

	Stack.Pop(ItemTags::State_Item_SpeedStar);
	TestNull(TEXT("전부 꺼지면 오라가 남지 않는다"), Stack.Top());

	// 같은 아이템을 효과 중에 다시 써도 태그는 1에서 1로 머무르지만, 방어적으로 두 번 켜도
	// 한 칸만 차지해야 한다. 두 칸이 되면 한 번 꺼도 오라가 남는다.
	Stack.Push(ItemTags::State_Item_SpeedStar, Speed);
	Stack.Push(ItemTags::State_Item_SpeedStar, Speed);
	Stack.Pop(ItemTags::State_Item_SpeedStar);
	TestNull(TEXT("같은 태그를 두 번 켜도 한 번 끄면 사라진다"), Stack.Top());

	return true;
}

#endif
