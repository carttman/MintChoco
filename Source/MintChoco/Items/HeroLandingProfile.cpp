#include "Items/HeroLandingProfile.h"

#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

void UHeroLandingProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!Burst.Paintball, LogMintChoco, Warning, TEXT("%s: %s has no Burst.Paintball, the landing will not paint."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!AimMarkerClass, LogMintChoco, Warning, TEXT("%s: %s has no AimMarkerClass, the landing spot will not be shown."), *GetNameSafe(Owner), *GetName());
}
