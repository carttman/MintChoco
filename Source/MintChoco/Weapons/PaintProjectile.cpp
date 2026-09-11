#include "Weapons/PaintProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "MeshScale.h"
#include "Weapons/PaintballProfile.h"

namespace
{
	/** Custom Primitive Data slots the body material reads; the contract is documented on APaintProjectile. */
	constexpr int32 BodySpeedIndex = 0;
	constexpr int32 BodyPhaseIndex = 1;
	constexpr int32 BodyBirthTimeIndex = 2;

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
	// BlockAllDynamic hits paintable meshes and pawns alike. The camera probe is excused so a ball
	// never shoves the spring arm, and other balls are excused because every pellet of a shot
	// leaves the same muzzle point: overlapping at birth, they would otherwise hit each other on
	// their first move and die before flying.
	Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Sphere->SetCollisionObjectType(PaintballChannel);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(PaintballChannel, ECR_Ignore);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->OnComponentHit.AddDynamic(this, &APaintProjectile::OnHit);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The body material stretches a tail behind the sphere, past the mesh's own bounds.
	Mesh->BoundsScale = 2.0f;

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

	if (UMaterialInterface* const TeamMaterial = GetTeamMaterial(PaintId))
	{
		Mesh->SetMaterial(0, TeamMaterial);
	}
	// Per-ball values ride Custom Primitive Data so every ball of a team shares one material. The
	// phase comes from the seed, so a client's cosmetic ball wobbles in step with the server's.
	Mesh->SetCustomPrimitiveDataFloat(BodySpeedIndex, Movement->InitialSpeed);
	Mesh->SetCustomPrimitiveDataFloat(BodyPhaseIndex, FRandomStream(Seed).GetFraction());
	Mesh->SetCustomPrimitiveDataFloat(BodyBirthTimeIndex, GetWorld()->GetTimeSeconds());

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

UMaterialInterface* APaintProjectile::GetTeamMaterial(uint8 InPaintId) const
{
	return TeamMaterials.IsValidIndex(InPaintId) ? TeamMaterials[InPaintId].Get() : nullptr;
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
