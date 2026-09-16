#include "Weapons/PaintSplashTestActor.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintballProfile.h"

APaintSplashTestActor::APaintSplashTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void APaintSplashTestActor::BeginPlay()
{
	Super::BeginPlay();
	Fire();
	if (Interval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(FireTimer, this, &APaintSplashTestActor::Fire, Interval, /*bLoop=*/true);
	}
}

void APaintSplashTestActor::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(FireTimer);
	Super::EndPlay(Reason);
}

void APaintSplashTestActor::Fire()
{
	UWorld* const World = GetWorld();
	if (!World || !Paintball)
	{
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector Direction = IncidentVelocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::DownVector);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintSplashTest), /*bTraceComplex=*/false, this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, Start + Direction * TraceLength, PaintballChannel, Params))
	{
		return;
	}

	// The same two halves as APaintProjectile::OnHit: the authority deposits, every machine plays.
	const int32 Seed = FMath::Rand();
	if (HasAuthority())
	{
		Paintball->Deposit.ApplyHit(World, Hit, IncidentVelocity, PaintId, Seed, /*Charge=*/1.0f, /*StarGen=*/0, Paintball->Radius);
	}
	Paintball->PlayImpactEffect(*World, Hit, IncidentVelocity, PaintId, Seed);
}
