#include "Weapons/PaintballProfile.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Paint/PaintLog.h"
#include "Weapons/PaintProjectile.h"

void UPaintballProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!ProjectileClass, LogPaint, Warning, TEXT("%s: %s has no ProjectileClass, it cannot be fired."),
		*GetNameSafe(Owner), *GetName());
	UE_CLOG(!Deposit.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no BrushProfile, its hits will not paint."),
		*GetNameSafe(Owner), *GetName());
}

APaintProjectile* UPaintballProfile::Launch(UWorld& World, const FTransform& SpawnTransform, APawn* Instigator,
	const FVector& Velocity, uint8 PaintId, int32 Seed) const
{
	if (!ProjectileClass)
	{
		return nullptr;
	}

	// Deferred so the movement component reads its velocity when it initializes, not after.
	APaintProjectile* const Projectile = World.SpawnActorDeferred<APaintProjectile>(
		ProjectileClass, SpawnTransform, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->Init(this, PaintId, Seed, Velocity);
	Projectile->FinishSpawning(SpawnTransform);
	return Projectile;
}
