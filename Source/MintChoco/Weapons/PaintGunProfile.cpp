#include "Weapons/PaintGunProfile.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Paint/PaintLog.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintballProfile.h"

void UPaintGunProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!Paintball, LogPaint, Warning, TEXT("%s: %s has no Paintball, it will not fire."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!Scatter, LogPaint, Warning, TEXT("%s: %s has no Scatter, it will not fire."), *GetNameSafe(Owner), *GetName());
	if (Paintball)
	{
		Paintball->LogUnsetReferences(Owner);
	}
}

bool UPaintGunProfile::Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const
{
	if (!Context.World || !Paintball || !Scatter)
	{
		return false;
	}

	// The player aims with the camera, not the barrel: find what the crosshair rests on and
	// converge the barrel onto it, so the ball lands where the player is looking.
	const FVector MuzzleLocation = Context.Muzzle.GetLocation();
	FVector AimPoint = Context.ViewOrigin + Context.ViewDirection * AimTraceDistance;
	{
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintGunAim), /*bTraceComplex=*/false, Context.Instigator);
		if (Context.World->LineTraceSingleByChannel(Hit, Context.ViewOrigin, AimPoint, ECC_Visibility, Params))
		{
			AimPoint = Hit.ImpactPoint;
		}
	}

	// A target closer than the muzzle (a wall the character is pressed against) would send the
	// ball backwards; the view direction is the honest fallback there.
	FVector Direction = AimPoint - MuzzleLocation;
	if (Direction.SizeSquared() < FMath::Square(10.0f) || FVector::DotProduct(Direction, Context.ViewDirection) <= 0.0f)
	{
		Direction = Context.ViewDirection;
	}

	OutShot.Muzzle = MuzzleLocation;
	OutShot.Direction = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutShot.Seed = Context.Seed;
	OutShot.PaintId = Context.PaintId;

	// Without authority the shot is only described; the server launches the ball that paints,
	// and the weapon replays this description as a cosmetic one.
	if (!Context.bAuthority)
	{
		return true;
	}
	return Launch(*Context.World, Context.Instigator, OutShot, /*bCosmetic=*/false);
}

void UPaintGunProfile::PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const
{
	if (Paintball && Scatter)
	{
		Launch(World, Instigator, Shot, /*bCosmetic=*/true);
	}
}

bool UPaintGunProfile::Launch(UWorld& World, APawn* Instigator, const FPaintShot& Shot, bool bCosmetic) const
{
	TArray<FVector> Directions;
	Scatter->ComputePelletDirections(Shot.Direction, Shot.Seed, Directions);

	bool bLaunched = false;
	for (int32 Pellet = 0; Pellet < Directions.Num(); ++Pellet)
	{
		// The first pellet keeps the shot seed so a pinned debug seed still pins its splat; the
		// rest derive from it and stay just as replayable.
		const int32 PelletSeed = Pellet == 0
			? Shot.Seed
			: static_cast<int32>(HashCombineFast(static_cast<uint32>(Shot.Seed), static_cast<uint32>(Pellet)));
		const FTransform SpawnTransform(Directions[Pellet].Rotation(), Shot.Muzzle);
		bLaunched |= Paintball->Launch(World, SpawnTransform, Instigator,
			Directions[Pellet] * Scatter->MuzzleSpeed, Shot.PaintId, PelletSeed, bCosmetic) != nullptr;
	}
	return bLaunched;
}
