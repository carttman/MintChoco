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
	PrimaryActorTick.bCanEverTick = false;
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
