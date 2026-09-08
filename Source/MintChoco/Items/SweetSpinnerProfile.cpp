#include "Items/SweetSpinnerProfile.h"

#include "MintChoco.h"
#include "Weapons/PaintGunProfile.h"

void USweetSpinnerProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!Volley, LogMintChoco, Warning, TEXT("%s: %s has no Volley, the spinner will spin without painting."), *GetNameSafe(Owner), *GetName());
	if (Volley)
	{
		Volley->LogUnsetReferences(Owner);
	}
}
