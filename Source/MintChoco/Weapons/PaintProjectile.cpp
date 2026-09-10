#include "Weapons/PaintProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "MeshScale.h"
#include "Game/TeamTypes.h"
#include "Game/Unit.h"
#include "Paint/PaintSplat.h"
#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintWeaponComponent.h"
#include "Weapons/PaintballProfile.h"

namespace
{
	UPrimitiveComponent* GetMovingBody(const APawn* Pawn)
	{
		return Pawn ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	}
}

APaintProjectile::APaintProjectile()
{
	// A plain ball needs no tick of its own. Only a ball that paints as it flies turns it on, in
	// Init, once the profile is known; bCanEverTick has to be set here for that to be possible.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	InitialLifeSpan = 5.0f;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(6.0f);
	// Blocks paintable meshes and hit receivers, but only *overlaps* pawns: a ball still dies and
	// reports the contact the moment it touches a pawn, yet it never blocks or shoves the pawn (a
	// blocking ball is a body the character movement depenetrates from). Query only, no physics body,
	// for the same reason. The camera probe is excused so a ball never shoves the spring arm, and
	// other balls are excused because every pellet of a shot leaves the same muzzle point:
	// overlapping at birth, they would otherwise hit each other on their first move and die before flying.
	Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(PaintballChannel);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(PaintballChannel, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->OnComponentHit.AddDynamic(this, &APaintProjectile::OnHit);
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &APaintProjectile::OnPawnOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bInitialVelocityInLocalSpace = false;
}

void APaintProjectile::Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity, bool bInCosmetic)
{
	check(InProfile);
	Profile = InProfile;
	PaintId = InPaintId;
	Seed = InSeed;
	bCosmetic = bInCosmetic;

	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = 0.0f;
	Movement->Velocity = Velocity;
	Movement->ProjectileGravityScale = Profile->GravityScale;

	Sphere->SetSphereRadius(Profile->Radius);
	ScaleMeshToRadius(Mesh, Profile->Radius);

	// A ball leaves the muzzle inside the shooter's reach; neither body may collide with the other.
	if (APawn* const Shooter = GetInstigator())
	{
		Sphere->IgnoreActorWhenMoving(Shooter, true);
		if (UPrimitiveComponent* const Body = GetMovingBody(Shooter))
		{
			Body->IgnoreActorWhenMoving(this, true);
		}
	}

	// Splats reach the other machines through the game state's log, so only the server's real ball
	// may paint. A cosmetic ball ticking here would draw the same trail a second time.
	LastTrailLocation = GetActorLocation();
	SetActorTickEnabled(!bCosmetic && HasAuthority() && Profile->HasTrail());
}

void APaintProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	// The shooter's ignore list would otherwise grow by one dead entry per shot fired.
	if (UPrimitiveComponent* const Body = GetMovingBody(GetInstigator()))
	{
		Body->IgnoreActorWhenMoving(this, false);
	}
	Super::EndPlay(Reason);
}

void APaintProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Profile || TrailSplatCount >= Profile->MaxTrailSplats)
	{
		SetActorTickEnabled(false);
		return;
	}

	// Measured along the path actually flown rather than from the muzzle, so a lobbed arc is
	// sampled at even spacing along its curve instead of bunching up near the apex.
	const FVector Location = GetActorLocation();
	TrailDistance += FVector::Dist(LastTrailLocation, Location);
	LastTrailLocation = Location;
	if (TrailDistance < Profile->TrailSpacing)
	{
		return;
	}

	// One sample per frame at most: a ball moving several spacings in one frame would otherwise
	// fire a burst of traces to catch up, which is exactly the cost the spacing exists to bound.
	TrailDistance -= Profile->TrailSpacing;
	TrailSplatCount += PaintTrailSample(Location, TrailSampleCount++);
}

int32 APaintProjectile::PaintTrailSample(const FVector& Location, int32 SampleIndex)
{
	UWorld* const World = GetWorld();
	const FVector Forward = Movement->Velocity.GetSafeNormal();
	if (!World || Forward.IsNearlyZero())
	{
		return 0;
	}

	// The rays lie in the plane across the flight direction, so one ray count covers floor,
	// ceiling and both sides however the ball is heading - no special case for "below".
	FVector RayUp;
	FVector RaySide;
	Forward.FindBestAxisVectors(RayUp, RaySide);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintProjectileTrail), /*bTraceComplex=*/false, this);
	Params.AddIgnoredActor(GetInstigator());

	const int32 Rays = Profile->TrailRayCount;
	const int32 Budget = Profile->MaxTrailSplats - TrailSplatCount;
	// Turning the fan by a per-sample amount keeps successive samples from lining their rays up
	// into a visible ladder. It comes from the seed, so a replay lands the marks in the same places.
	const float Twist = 2.0f * UE_PI * static_cast<float>(HashCombineFast(static_cast<uint32>(Seed), static_cast<uint32>(SampleIndex)) & 0xFFFF) / 65536.0f;

	int32 Painted = 0;
	for (int32 Index = 0; Index < Rays && Painted < Budget; ++Index)
	{
		const float Angle = Twist + 2.0f * UE_PI * static_cast<float>(Index) / static_cast<float>(Rays);
		const FVector Direction = RayUp * FMath::Cos(Angle) + RaySide * FMath::Sin(Angle);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Location, Location + Direction * Profile->TrailRadius, PaintballChannel, Params)
			|| Hit.bStartPenetrating)
		{
			continue;
		}

		// A pawn is never painted by a trail: the mark would not stick to it, and ApplyHit would
		// also strike it as a paint hit receiver once per sample.
		if (Cast<APawn>(Hit.GetActor()))
		{
			continue;
		}

		if (Profile->bTrailSkipTransient && !FPaintDeposit::KeepsPaint(Hit))
		{
			continue;
		}

		// The incident velocity runs along the ray, so the brush stretches the stamp away from the
		// path and the samples read as one stripe rather than a row of dots.
		const int32 RaySeed = static_cast<int32>(HashCombineFast(
			static_cast<uint32>(Seed), static_cast<uint32>(SampleIndex * Rays + Index + 1)));
		if (Profile->TrailDeposit.ApplyHit(World, Hit, Direction * Movement->Velocity.Size(), PaintId, RaySeed))
		{
			++Painted;
		}
	}
	return Painted;
}

void APaintProjectile::OnPawnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only pawns overlap; everything else blocks and arrives through OnHit. The shooter is already
	// on the ignore list, so this is another pawn. A sweep result carries the impact point; a
	// non-sweep overlap (the pawn walked into a ball) uses the ball itself as the contact.
	if (!Cast<APawn>(OtherActor))
	{
		return;
	}

	// A ball of the pawn's own colour passes through: the burst of an item lands on the user who
	// fired it (and on teammates), and those balls have no instigator to be excused by the ignore list.
	if (const AUnit* const Unit = Cast<AUnit>(OtherActor))
	{
		const int32 Team = Unit->GetTeam();
		const uint8 UnitPaintId = Teams::IsValidId(Team)
			? static_cast<uint8>(Team)
			: (Unit->GetPaintWeapon() ? Unit->GetPaintWeapon()->GetPaintId() : PaintIdNone);
		if (UnitPaintId == PaintId)
		{
			return;
		}
	}

	FHitResult Hit = SweepResult;
	if (!bFromSweep)
	{
		Hit.ImpactPoint = GetActorLocation();
		Hit.Location = GetActorLocation();
		Hit.ImpactNormal = -Movement->Velocity.GetSafeNormal();
		Hit.Normal = Hit.ImpactNormal;
		Hit.HitObjectHandle = FActorInstanceHandle(OtherActor);
	}
	OnHit(nullptr, OtherActor, nullptr, FVector::ZeroVector, Hit);
}

void APaintProjectile::OnHit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}
	if (Profile && !bCosmetic)
	{
		// The hit fires from inside the move, before the movement component zeroes its velocity,
		// so this is still the impact velocity; the fallback covers a blocked first step.
		FVector Velocity = Movement->Velocity;
		if (Velocity.IsNearlyZero())
		{
			Velocity = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal() * Movement->InitialSpeed;
		}
		Profile->Deposit.ApplyHit(GetWorld(), Hit, Velocity, PaintId, Seed);
	}

	Destroy();
}
