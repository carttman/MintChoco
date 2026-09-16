#include "Items/HoneyBalloonProfile.h"

#include "Game/Unit.h"
#include "Items/HoneyBalloonProjectile.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

FVector UHoneyBalloonProfile::GetThrowDirection(const AUnit& Unit)
{
	return Unit.GetControlRotation().Vector();
}

FVector UHoneyBalloonProfile::GetThrowOrigin(const AUnit& Unit, const FVector& Direction) const
{
	return Unit.GetPawnViewLocation() + Direction * ThrowOffset;
}

FPaintBurstParams UHoneyBalloonProfile::MakeBurstParams(uint8 PaintId, int32 Seed) const
{
	FPaintBurstParams Params = Burst;
	Params.PaintId = PaintId;
	Params.Seed = Seed;
	return Params;
}

float UHoneyBalloonProfile::GetProjectileRadius() const
{
	const AHoneyBalloonProjectile* const Defaults = ProjectileClass ? ProjectileClass->GetDefaultObject<AHoneyBalloonProjectile>() : nullptr;
	return Defaults ? Defaults->GetCollisionRadius() : 0.0f;
}

void UHoneyBalloonProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!ProjectileClass, LogMintChoco, Warning, TEXT("%s: %s has no ProjectileClass, nothing will be thrown."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!Burst.Paintball, LogMintChoco, Warning, TEXT("%s: %s has no Burst.Paintball, the balloon will not paint."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!AimPreview.DotMesh, LogMintChoco, Warning, TEXT("%s: %s has no AimPreview.DotMesh, the throw arc falls back to debug lines."), *GetNameSafe(Owner), *GetName());
}
