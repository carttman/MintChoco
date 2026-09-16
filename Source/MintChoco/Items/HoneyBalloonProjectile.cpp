#include "Items/HoneyBalloonProjectile.h"

#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Game/Unit.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/ItemAreaEffect.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"

void AHoneyBalloonProjectile::SetProfile(const UHoneyBalloonProfile* InProfile)
{
	Profile = InProfile;
	if (Profile && Movement)
	{
		Movement->ProjectileGravityScale = Profile->GravityScale;
	}
}

void AHoneyBalloonProjectile::HandleUnitOverlap(AUnit& Unit)
{
	// 아군에 닿아도 터진다. 스턴은 광역 규칙이 상대만 고른다.
	Detonate();
}

void AHoneyBalloonProjectile::OnDetonate()
{
	UWorld* const World = GetWorld();
	if (!World || !Profile)
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	APaintBurst::Spawn(*World, Origin, Profile->MakeBurstParams(GetPaintId(), FMath::Rand()));

	FItemAreaEffect::Apply(*World, Origin, Profile->StunRadius, GetTeam(), GetInstigatorUnit(), /*bKnockback=*/false);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀풍선이 터졌다."), *GetNameSafe(GetInstigator()));
}
