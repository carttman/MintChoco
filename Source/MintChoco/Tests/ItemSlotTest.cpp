#include "Misc/AutomationTest.h"

#include "Items/ItemSlotComponent.h"
#include "Items/SpeedStarProfile.h"
#include "Items/SweetSpinnerProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 델리게이트를 세는 도우미. 동적 델리게이트는 UFUNCTION이 필요하므로 UObject여야 한다. */
	class FSlotListener
	{
	public:
		int32 Changes = 0;
		UItemProfile* Last = nullptr;
	};
}

/** 소유자 없는 슬롯(테스트)은 권한이 있는 것으로 치고, 교체·소비·사용 실패를 ASC 없이 검사한다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemSlotHeldTest,
	"MintChoco.Items.Slot.Held",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemSlotHeldTest::RunTest(const FString& Parameters)
{
	UItemSlotComponent* const Slot = NewObject<UItemSlotComponent>();
	USweetSpinnerProfile* const Spinner = NewObject<USweetSpinnerProfile>();
	USpeedStarProfile* const Star = NewObject<USpeedStarProfile>();

	TestNull(TEXT("starts empty"), Slot->GetHeldItem());
	TestFalse(TEXT("nothing to use"), Slot->TryUseHeldItem());
	TestFalse(TEXT("no boost without an item"), Slot->IsSpeedBoostAuthorized());

	Slot->GiveItem(Spinner);
	TestEqual(TEXT("holds the first item"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Spinner));
	TestFalse(TEXT("spinner does not authorize the boost"), Slot->IsSpeedBoostAuthorized());

	Slot->GiveItem(Star);
	TestEqual(TEXT("a new pickup replaces the held one"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Star));
	TestTrue(TEXT("holding a speed star authorizes the boost flag"), Slot->IsSpeedBoostAuthorized());

	TestFalse(TEXT("use without an ability system fails"), Slot->TryUseHeldItem());
	TestEqual(TEXT("a failed use keeps the item"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Star));

	Slot->ConsumeHeldItem();
	TestNull(TEXT("consumed"), Slot->GetHeldItem());

	Slot->GiveItem(nullptr);
	TestNull(TEXT("null is not an item"), Slot->GetHeldItem());

	return true;
}

#endif
