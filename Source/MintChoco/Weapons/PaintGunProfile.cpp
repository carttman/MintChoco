#include "Weapons/PaintGunProfile.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Paint/PaintLog.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintballProfile.h"

namespace
{
	/**
	 * Sweeps a ball of Radius from Position with Velocity under one gravity for Seconds. Returns
	 * true on a hit with OutImpact filled; otherwise leaves Position and Velocity where the flight
	 * ended so the next phase can carry on from there.
	 */
	bool SimulateBall(UWorld& World, FVector& Position, FVector& Velocity, float GravityScale, float Seconds, float Radius, AActor* Ignore, FVector& OutImpact)
	{
		FPredictProjectilePathParams Params(Radius, Position, Velocity, Seconds);
		Params.bTraceWithCollision = true;
		Params.bTraceComplex = false;
		Params.bTraceWithChannel = false;
		// What a ball stops on: world geometry and pawns. Pawns really overlap, but the flight ends there.
		Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
		Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		Params.SimFrequency = 20.0f;
		if (Ignore)
		{
			Params.ActorsToIgnore.Add(Ignore);
		}
		// 0 means "do not override", so a straight flight needs a gravity that is merely negligible.
		Params.OverrideGravityZ = FMath::IsNearlyZero(GravityScale) ? -UE_KINDA_SMALL_NUMBER : World.GetGravityZ() * GravityScale;

		FPredictProjectilePathResult Result;
		if (UGameplayStatics::PredictProjectilePath(&World, Params, Result))
		{
			OutImpact = Result.HitResult.ImpactPoint;
			return true;
		}
		Position = Result.LastTraceDestination.Location;
		Velocity = Result.LastTraceDestination.Velocity;
		return false;
	}
}

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

	FVector AimPoint;
	FVector Direction;
	ComputeAim(Context, AimPoint, Direction);

	OutShot.Muzzle = Context.Muzzle.GetLocation();
	OutShot.VisualMuzzle = Context.VisualMuzzle.Get(Context.Muzzle.GetLocation());
	OutShot.Direction = Direction;
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

void UPaintGunProfile::ComputeAim(const FPaintFireContext& Context, FVector& OutAimPoint, FVector& OutDirection) const
{
	// The player aims with the camera, not the barrel: find what the crosshair rests on and
	// converge the barrel onto it, so the ball lands where the player is looking.
	const FVector MuzzleLocation = Context.Muzzle.GetLocation();
	OutAimPoint = Context.ViewOrigin + Context.ViewDirection * AimTraceDistance;
	{
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintGunAim), /*bTraceComplex=*/false, Context.Instigator);
		if (Context.World->LineTraceSingleByChannel(Hit, Context.ViewOrigin, OutAimPoint, ECC_Visibility, Params))
		{
			OutAimPoint = Hit.ImpactPoint;
		}
	}

	// A target closer than the muzzle (a wall the character is pressed against) would send the
	// ball backwards; the view direction is the honest fallback there.
	FVector Direction = OutAimPoint - MuzzleLocation;
	if (Direction.SizeSquared() < FMath::Square(10.0f) || FVector::DotProduct(Direction, Context.ViewDirection) <= 0.0f)
	{
		Direction = Context.ViewDirection;
	}
	OutDirection = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

bool UPaintGunProfile::PredictImpact(const FPaintFireContext& Context, FVector& OutAimPoint, FVector& OutImpact) const
{
	if (!Context.World || !Paintball || !Scatter)
	{
		return false;
	}

	FVector Direction;
	ComputeAim(Context, OutAimPoint, Direction);

	FVector Position = Context.Muzzle.GetLocation();
	FVector Velocity = Direction * Scatter->MuzzleSpeed;

	// The ball flies straight-ish for DropAfter, then under the drop gravity until its life ends
	// (APaintProjectile::Init runs the same switch off a timer).
	const float StraightSeconds = Paintball->DropAfter > 0.0f ? FMath::Min(Paintball->DropAfter, PaintballLifeSpanSeconds) : PaintballLifeSpanSeconds;
	if (SimulateBall(*Context.World, Position, Velocity, Paintball->GravityScale, StraightSeconds, Paintball->Radius, Context.Instigator, OutImpact))
	{
		return true;
	}
	const float DropSeconds = PaintballLifeSpanSeconds - StraightSeconds;
	if (DropSeconds > 0.0f
		&& SimulateBall(*Context.World, Position, Velocity, Paintball->DropGravityScale, DropSeconds, Paintball->Radius, Context.Instigator, OutImpact))
	{
		return true;
	}
	// Nothing in the way: the ball dies in the air, and that is still where the shot ends.
	OutImpact = Position;
	return true;
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

	// The physics flies from Shot.Muzzle; only the mesh starts at the gun and slides onto the path.
	const FVector VisualOffset = FVector(Shot.VisualMuzzle) - FVector(Shot.Muzzle);

	bool bLaunched = false;
	for (int32 Pellet = 0; Pellet < Directions.Num(); ++Pellet)
	{
		// The first pellet keeps the shot seed so a pinned debug seed still pins its splat; the
		// rest derive from it and stay just as replayable.
		const int32 PelletSeed = Pellet == 0
			? Shot.Seed
			: static_cast<int32>(HashCombineFast(static_cast<uint32>(Shot.Seed), static_cast<uint32>(Pellet)));
		const FTransform SpawnTransform(Directions[Pellet].Rotation(), Shot.Muzzle);
		APaintProjectile* const Projectile = Paintball->Launch(World, SpawnTransform, Instigator,
			Directions[Pellet] * Scatter->MuzzleSpeed, Shot.PaintId, PelletSeed, bCosmetic, /*DropAfterOverride=*/-1.0f, VisualOffset);
		bLaunched |= Projectile != nullptr;

		// 첫 탄에만 소리를 남긴다. 펠릿이 거의 동시에 닿아 같은 소리가 겹치기 때문이다.
		if (Projectile && bImpactSoundOncePerShot && Pellet != 0)
		{
			Projectile->SetPlaysImpactSound(false);
		}
	}
	return bLaunched;
}
