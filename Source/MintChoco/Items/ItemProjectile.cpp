#include "Items/ItemProjectile.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "Game/Unit.h"
#include "Weapons/PaintProjectile.h"

AItemProjectile::AItemProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);
	NetPriority = 3.0f;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(25.0f);
	// 월드에는 막히고, 폰과는 겹친다(폰의 Block과 min을 취하면 Overlap이 된다). 카메라와
	// 페인트탄은 지나간다. 꿀벌은 페인트탄에 맞아야 하므로 자기 생성자에서 Block으로 바꾼다.
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(PaintballChannel, ECR_Ignore);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->OnComponentHit.AddDynamic(this, &AItemProjectile::OnSphereHit);
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AItemProjectile::OnSphereBeginOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->ProjectileGravityScale = 1.0f;
	// 넷 업데이트마다 스냅하지 않고 메시를 목표로 끌어간다.
	Movement->bInterpMovement = true;
}

void AItemProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Movement && Mesh)
	{
		Movement->SetInterpolatedComponent(Mesh);
	}
}

void AItemProjectile::Init(AUnit* InInstigator, int32 InTeam, const FVector& Velocity)
{
	SetInstigator(InInstigator);
	Team = InTeam;
	InitialVelocity = Velocity;

	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = 0.0f;
	Movement->Velocity = Velocity;

	// 던진 사람 곁에서 태어나므로 서로 부딪히지 않는다.
	if (InInstigator)
	{
		Sphere->IgnoreActorWhenMoving(InInstigator, true);
		if (UPrimitiveComponent* const Body = InInstigator->GetCapsuleComponent())
		{
			Body->IgnoreActorWhenMoving(this, true);
		}
	}
}

void AItemProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// 클라이언트 복사본은 그림이다. 판정도 없고 벽에도 걸리지 않는다: 서버가 벽에 닿아
		// 없애기까지 RTT/2만큼 더 날 뿐이다. 속도는 첫 넷 업데이트 전까지 초기값으로 간다.
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Movement->Velocity = InitialVelocity;
		if (AUnit* const Thrower = GetInstigatorUnit())
		{
			if (UPrimitiveComponent* const Body = Thrower->GetCapsuleComponent())
			{
				Body->IgnoreActorWhenMoving(this, true);
			}
		}
	}
}

void AItemProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	if (const AUnit* const Thrower = GetInstigatorUnit())
	{
		if (UPrimitiveComponent* const Body = Thrower->GetCapsuleComponent())
		{
			Body->IgnoreActorWhenMoving(this, false);
		}
	}
	Super::EndPlay(Reason);
}

void AItemProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AItemProjectile, Team, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AItemProjectile, InitialVelocity, COND_InitialOnly);
}

void AItemProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (Movement)
	{
		Movement->Velocity = NewVelocity;
	}
}

void AItemProjectile::PostNetReceiveLocationAndRotation()
{
	if (!Movement || !Movement->UpdatedComponent)
	{
		Super::PostNetReceiveLocationAndRotation();
		return;
	}

	// 콜리전은 목표로 옮기고 메시는 보간으로 따라간다. 기본 구현의 스냅 대신이다.
	const FRepMovement& Rep = GetReplicatedMovement();
	const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(Rep.Location, this);
	Movement->MoveInterpolationTarget(NewLocation, Rep.Rotation);
}

AUnit* AItemProjectile::GetInstigatorUnit() const
{
	return Cast<AUnit>(GetInstigator());
}

void AItemProjectile::HandleWorldHit(const FHitResult& Hit)
{
	Detonate();
}

void AItemProjectile::Detonate()
{
	if (bDetonated || !HasAuthority())
	{
		return;
	}
	bDetonated = true;
	OnDetonate();
	Destroy();
}

void AItemProjectile::OnSphereHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (!HasAuthority() || bDetonated)
	{
		return;
	}
	HandleWorldHit(Hit);
}

void AItemProjectile::OnSphereBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || bDetonated)
	{
		return;
	}
	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (!Unit || Unit == GetInstigator() || OtherComp != Unit->GetCapsuleComponent())
	{
		return;
	}
	HandleUnitOverlap(*Unit);
}
