#include "Items/HoneyBalloonAbility.h"

#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/HoneyBalloonProjectile.h"
#include "MintChoco.h"

void UGA_HoneyBalloon::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Honey || !Honey->ProjectileClass || !World || !IsAuthority())
	{
		return;
	}

	// 눈높이에서 시선 방향으로. 서버의 컨트롤 회전은 무브마다 갱신되므로 소유자가 본 것과 같다.
	const FVector Direction = Unit.GetControlRotation().Vector();
	const FVector Origin = Unit.GetPawnViewLocation() + Direction * 60.0f;
	const FTransform SpawnTransform(Direction.Rotation(), Origin);

	AHoneyBalloonProjectile* const Balloon = World->SpawnActorDeferred<AHoneyBalloonProjectile>(
		Honey->ProjectileClass, SpawnTransform, &Unit, &Unit, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Balloon)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 꿀풍선을 스폰하지 못했다."), *GetNameSafe(&Unit));
		return;
	}
	Balloon->Init(&Unit, Unit.GetTeam(), Direction * Honey->ThrowSpeed);
	Balloon->SetProfile(Honey);
	Balloon->FinishSpawning(SpawnTransform);
}
