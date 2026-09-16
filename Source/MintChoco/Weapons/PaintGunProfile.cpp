#include "Weapons/PaintGunProfile.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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

float UPaintGunProfile::ComputePelletDropAfter(int32 Pellet, int32 PelletCount) const
{
	// -1 은 "프로필의 DropAfter를 그대로 쓰라"는 뜻이다. 0 은 "즉시 꺾여라"라서 쓸 수 없다.
	if (!bStaggerPelletDrop || !Scatter)
	{
		return -1.0f;
	}

	// 부채꼴에서의 좌우 위치: 가운데가 0, 양끝이 1. 펠릿은 -Fan에서 +Fan으로 고르게 놓이므로
	// 인덱스만으로 구할 수 있다(UPaintScatterProfile::ComputePelletDirections).
	const float Lateral = PelletCount > 1
		? FMath::Abs(2.0f * static_cast<float>(Pellet) / static_cast<float>(PelletCount - 1) - 1.0f)
		: 0.0f;

	// 가운데(Lateral 0)가 Far, 바깥(Lateral 1)이 Near.
	const float TargetDistance = FMath::Lerp(PelletDropFarDistance, PelletDropNearDistance, Lateral);
	const float StraightDistance = FMath::Max(TargetDistance - PelletDropLead, 0.0f);
	return StraightDistance / FMath::Max(Scatter->MuzzleSpeed, 1.0f);
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
		const float DropAfter = ComputePelletDropAfter(Pellet, Directions.Num());
		APaintProjectile* const Projectile = Paintball->Launch(World, SpawnTransform, Instigator,
			Directions[Pellet] * Scatter->MuzzleSpeed, Shot.PaintId, PelletSeed, bCosmetic,
			DropAfter, VisualOffset);
		bLaunched |= Projectile != nullptr;

		// 첫 탄에만 소리를 남긴다. 펠릿이 거의 동시에 닿아 같은 소리가 겹치기 때문이다.
		if (Projectile && bImpactSoundOncePerShot && Pellet != 0)
		{
			Projectile->SetPlaysImpactSound(false);
		}

		ScheduleTrailingPainters(World, Instigator, Shot, Directions[Pellet], PelletSeed, DropAfter, bCosmetic);
	}
	return bLaunched;
}

void UPaintGunProfile::ScheduleTrailingPainters(UWorld& World, APawn* Instigator, const FPaintShot& Shot,
	const FVector& Direction, int32 PelletSeed, float DropAfter, bool bCosmetic) const
{
	if (!PainterPaintball || PainterCount <= 0 || PainterDelay <= 0.0f || !Scatter)
	{
		return;
	}

	// 람다에 담는 것은 전부 값이거나 애셋 포인터다. 데이터 애셋은 레벨보다 오래 살지만 월드와
	// 폰은 아니므로 그 둘만 약참조로 들고, 발사 전에 사라졌으면 조용히 접는다.
	const TWeakObjectPtr<UWorld> WeakWorld(&World);
	const TWeakObjectPtr<APawn> WeakInstigator(Instigator);
	const UPaintballProfile* const Painter = PainterPaintball;
	const FVector Muzzle(Shot.Muzzle);
	const FVector Velocity = Direction * Scatter->MuzzleSpeed;
	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	const uint8 ShotPaintId = Shot.PaintId;

	for (int32 Step = 1; Step <= PainterCount; ++Step)
	{
		// 시드는 펠릿에서 갈라 나온다. 같은 사격을 다시 재생하면 같은 자국이 나온다.
		const int32 PainterSeed = static_cast<int32>(
			HashCombineFast(static_cast<uint32>(PelletSeed), static_cast<uint32>(0x9E37 + Step)));

		// 핸들을 들고 있지 않는 이유: 한 번 쏘고 끝나는 예약이고, 취소할 일이 없다.
		FTimerHandle Handle;
		World.GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateLambda(
				[WeakWorld, WeakInstigator, Painter, SpawnTransform, Velocity, ShotPaintId, PainterSeed, DropAfter, bCosmetic]()
				{
					if (UWorld* const LiveWorld = WeakWorld.Get())
					{
						Painter->Launch(*LiveWorld, SpawnTransform, WeakInstigator.Get(), Velocity,
							ShotPaintId, PainterSeed, bCosmetic, DropAfter);
					}
				}),
			PainterDelay * static_cast<float>(Step), /*bLoop=*/false);
	}
}
