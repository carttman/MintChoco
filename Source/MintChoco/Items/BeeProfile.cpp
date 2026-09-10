#include "Items/BeeProfile.h"

#include "Items/BeeProjectile.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

void UBeeProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!ProjectileClass, LogMintChoco, Warning, TEXT("%s: %s has no ProjectileClass, no bee will fly."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!Burst.Paintball, LogMintChoco, Warning, TEXT("%s: %s has no Burst.Paintball, the bee will not paint on impact."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!TrailDeposit.CanPaint(), LogMintChoco, Warning, TEXT("%s: %s has no TrailDeposit brush, the bee leaves no trail."), *GetNameSafe(Owner), *GetName());
}
