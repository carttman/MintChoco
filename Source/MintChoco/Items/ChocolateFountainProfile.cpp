#include "Items/ChocolateFountainProfile.h"

#include "Items/ChocolateFountain.h"
#include "MintChoco.h"

void UChocolateFountainProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!DomeClass, LogMintChoco, Warning, TEXT("%s: %s has no DomeClass, nothing will appear."), *GetNameSafe(Owner), *GetName());
}
