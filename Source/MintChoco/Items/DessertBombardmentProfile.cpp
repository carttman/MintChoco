#include "Items/DessertBombardmentProfile.h"

#include "Items/PaintRain.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

void UDessertBombardmentProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!Paintball, LogMintChoco, Warning, TEXT("%s: %s has no Paintball, nothing will fall."), *GetNameSafe(Owner), *GetName());
}
