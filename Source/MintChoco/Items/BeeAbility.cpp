#include "Items/BeeAbility.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "Game/Unit.h"
#include "Items/BeeProfile.h"
#include "Items/BeeProjectile.h"
#include "Items/ItemAreaEffect.h"
#include "MintChoco.h"

AUnit* UGA_Bee::FindNearestOpponent(const AUnit& From)
{
	AUnit* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AUnit> It(From.GetWorld()); It; ++It)
	{
		AUnit* const Candidate = *It;
		if (!Candidate || !FItemAreaEffect::ShouldAffect(Candidate->GetTeam(), From.GetTeam(), Candidate == &From))
		{
			continue;
		}
		const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), From.GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Candidate;
		}
	}
	return Nearest;
}

void UGA_Bee::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UBeeProfile* const Bee = Cast<UBeeProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Bee || !Bee->ProjectileClass || !World || !IsAuthority())
	{
		return;
	}

	const FVector Direction = Unit.GetControlRotation().Vector();
	const FVector Origin = Unit.GetPawnViewLocation() + Direction * 80.0f;
	const FTransform SpawnTransform(Direction.Rotation(), Origin);

	ABeeProjectile* const Projectile = World->SpawnActorDeferred<ABeeProjectile>(
		Bee->ProjectileClass, SpawnTransform, &Unit, &Unit, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 꿀벌을 스폰하지 못했다."), *GetNameSafe(&Unit));
		return;
	}
	Projectile->Init(&Unit, Unit.GetTeam(), Direction * Bee->Speed);
	Projectile->SetProfile(Bee);
	Projectile->SetTarget(FindNearestOpponent(Unit));
	Projectile->FinishSpawning(SpawnTransform);
}
