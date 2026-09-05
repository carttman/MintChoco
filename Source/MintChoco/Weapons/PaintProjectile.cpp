#include "Weapons/PaintProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Weapons/PaintballProfile.h"

namespace
{
	/** The engine's BasicShapes/Sphere is 100 cm across, so this maps a radius to its scale. */
	constexpr float BasicSphereRadius = 50.0f;

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
	// The project defines no projectile profile. BlockAllDynamic hits paintable meshes and pawns
	// alike; only the camera probe is excused so a ball never shoves the spring arm.
	Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->OnComponentHit.AddDynamic(this, &APaintProjectile::OnHit);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bInitialVelocityInLocalSpace = false;
}

void APaintProjectile::Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity)
{
	check(InProfile);
	Profile = InProfile;
	PaintId = InPaintId;
	Seed = InSeed;

	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = 0.0f;
	Movement->Velocity = Velocity;
	Movement->ProjectileGravityScale = Profile->GravityScale;

	Sphere->SetSphereRadius(Profile->Radius);
	Mesh->SetRelativeScale3D(FVector(Profile->Radius / BasicSphereRadius));

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

void APaintProjectile::OnHit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (Profile)
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
