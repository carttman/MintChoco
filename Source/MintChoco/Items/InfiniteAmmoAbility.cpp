#include "Items/InfiniteAmmoAbility.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"

UGA_InfiniteAmmo::UGA_InfiniteAmmo()
{
	StateTag = ItemTags::State_Item_InfiniteAmmo;
	EffectClass = UGE_InfiniteAmmo::StaticClass();
}

void UGA_InfiniteAmmo::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	if (UItemSlotComponent* const Slot = Unit.GetItemSlot())
	{
		Slot->RestartInkLook(Profile.Duration);
	}
}
