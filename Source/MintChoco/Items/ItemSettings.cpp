#include "Items/ItemSettings.h"

#include "Items/ItemPickup.h"
#include "Items/ItemProfile.h"

void UItemSettings::LoadItems(TArray<UItemProfile*>& OutItems) const
{
	OutItems.Reset();
	for (const TSoftObjectPtr<UItemProfile>& Soft : Items)
	{
		if (UItemProfile* const Item = Soft.LoadSynchronous())
		{
			OutItems.Add(Item);
		}
	}
}

UItemProfile* UItemSettings::FindItemByStateTag(const FGameplayTag& StateTag) const
{
	if (!StateTag.IsValid())
	{
		return nullptr;
	}

	TArray<UItemProfile*> Loaded;
	LoadItems(Loaded);
	for (UItemProfile* const Item : Loaded)
	{
		if (Item->GetStateTag() == StateTag)
		{
			return Item;
		}
	}
	return nullptr;
}

UClass* UItemSettings::LoadPickupClass() const
{
	return PickupClass.IsNull() ? nullptr : PickupClass.LoadSynchronous();
}
