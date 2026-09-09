#include "Items/HoneyBalloonProfile.h"

#include "Items/HoneyBalloonProjectile.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

void UHoneyBalloonProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!ProjectileClass, LogMintChoco, Warning, TEXT("%s: %s has no ProjectileClass, nothing will be thrown."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!Burst.Paintball, LogMintChoco, Warning, TEXT("%s: %s has no Burst.Paintball, the balloon will not paint."), *GetNameSafe(Owner), *GetName());
}
