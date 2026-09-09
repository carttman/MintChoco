#include "Weapons/PaintSniperProfile.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Paint/PaintLog.h"
#include "Weapons/PaintProjectile.h"

namespace
{
	/** The pawn a hit belongs to: the pawn itself, or the pawn that owns the hit actor (a carried item). */
	const APawn* GetHitPawn(const FHitResult& Hit)
	{
		const AActor* const Actor = Hit.GetActor();
		if (!Actor)
		{
			return nullptr;
		}
		if (const APawn* const Pawn = Cast<APawn>(Actor))
		{
			return Pawn;
		}
		return Cast<APawn>(Actor->GetOwner());
	}
}

UPaintSniperProfile::UPaintSniperProfile()
{
	FireMode = EPaintFireMode::Charged;
}

void UPaintSniperProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!Impact.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no Impact BrushProfile, the ray's end will not paint."),
		*GetNameSafe(Owner), *GetName());
	UE_CLOG(!Trail.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no Trail BrushProfile, the ground under the ray will not paint."),
		*GetNameSafe(Owner), *GetName());
}

bool UPaintSniperProfile::Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const
{
	if (!Context.World)
	{
		return false;
	}

	// The paintball channel is what a ball would hit: pawns block it (their capsule and mesh both
	// ignore Visibility), paintable meshes block it, balls in flight ignore it.
	const FVector TraceEnd = Context.ViewOrigin + Context.ViewDirection * Range;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintSniper), /*bTraceComplex=*/false, Context.Instigator);
	FHitResult Hit;
	const bool bHit = Context.World->LineTraceSingleByChannel(Hit, Context.ViewOrigin, TraceEnd, PaintballChannel, Params);
	const FVector End = bHit ? Hit.ImpactPoint : TraceEnd;
	const APawn* const Victim = bHit ? GetHitPawn(Hit) : nullptr;

	// The player aims with the camera, the ray is drawn and the trail laid from the barrel: converge
	// the two on the end point. A target closer than the muzzle would point the barrel backwards,
	// and the view direction is the honest fallback there.
	const FVector MuzzleLocation = Context.Muzzle.GetLocation();
	FVector Direction = End - MuzzleLocation;
	if (Direction.SizeSquared() < FMath::Square(10.0f) || FVector::DotProduct(Direction, Context.ViewDirection) <= 0.0f)
	{
		Direction = Context.ViewDirection;
	}
	const float Length = FVector::Dist(MuzzleLocation, End);

	OutShot.Muzzle = MuzzleLocation;
	OutShot.Direction = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutShot.Distance = Length;
	OutShot.Seed = Context.Seed;
	OutShot.PaintId = Context.PaintId;

	// Without authority the shot is only described; the server retraces with the same view.
	if (!Context.bAuthority)
	{
		return true;
	}

	if (Victim)
	{
		UE_LOG(LogPaint, Log, TEXT("%s sniped %s."), *GetNameSafe(Context.Instigator), *Victim->GetName());
	}
	else if (bHit)
	{
		Impact.ApplyHit(Context.World, Hit, Context.ViewDirection * NominalImpactSpeed, Context.PaintId, Context.Seed);
	}

	PaintTrail(*Context.World, MuzzleLocation, OutShot.Direction, Length, Context.Instigator, Victim, Context.PaintId, Context.Seed);

	// The shot multicast skips the authority, which shows its own tracer here instead.
	DrawTracer(*Context.World, MuzzleLocation, End);
	return true;
}

void UPaintSniperProfile::PaintTrail(UWorld& World, const FVector& Muzzle, const FVector& Direction, float Length,
	const APawn* Instigator, const AActor* Victim, uint8 PaintId, int32 Seed) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintSniperTrail), /*bTraceComplex=*/false, Instigator);
	Params.AddIgnoredActor(Victim);

	// The last sample stops half a spacing short of the end so the trail never stacks on the impact stamp.
	const float LastDistance = Length - TrailSpacing * 0.5f;
	// The incident velocity runs along the ray, so the brush stretches every stamp along the path and
	// the samples join into one stripe.
	const FVector IncidentVelocity = Direction * NominalImpactSpeed;

	int32 Index = 1;
	for (float Distance = TrailSpacing; Distance < LastDistance && Index <= MaxTrailSplats; Distance += TrailSpacing, ++Index)
	{
		const FVector Start = Muzzle + Direction * Distance;
		FHitResult Drop;
		if (!World.LineTraceSingleByChannel(Drop, Start, Start - FVector::UpVector * TrailDropHeight, PaintballChannel, Params)
			|| Drop.bStartPenetrating || GetHitPawn(Drop))
		{
			continue;
		}

		// The first sample keeps the shot seed so a pinned debug seed still pins its stamp; the rest
		// derive from it and stay just as replayable.
		const int32 SampleSeed = Index == 1
			? Seed
			: static_cast<int32>(HashCombineFast(static_cast<uint32>(Seed), static_cast<uint32>(Index)));
		Trail.ApplyHit(&World, Drop, IncidentVelocity, PaintId, SampleSeed);
	}
}

void UPaintSniperProfile::PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const
{
	DrawTracer(World, Shot.Muzzle, Shot.Muzzle + FVector(Shot.Direction) * Shot.Distance);
}

void UPaintSniperProfile::DrawTracer(const UWorld& World, const FVector& Start, const FVector& End)
{
	DrawDebugLine(&World, Start, End, FColor::White, /*bPersistentLines=*/false, /*LifeTime=*/1.0f, /*DepthPriority=*/0, /*Thickness=*/2.0f);
}
