#include "Items/ItemProfile.h"

#include "Items/ItemAbility.h"
#include "MintChoco.h"

FGameplayTag UItemProfile::GetStateTag() const
{
	return AbilityClass ? AbilityClass.GetDefaultObject()->GetStateTag() : FGameplayTag();
}

void UItemProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!AbilityClass, LogMintChoco, Warning, TEXT("%s: %s has no AbilityClass, using it will do nothing."), *GetNameSafe(Owner), *GetName());
}
