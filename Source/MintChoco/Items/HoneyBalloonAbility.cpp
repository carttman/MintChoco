#include "Items/HoneyBalloonAbility.h"

#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/HoneyBalloonProjectile.h"
#include "Items/ItemAimPreview.h"
#include "MintChoco.h"

void UGA_HoneyBalloon::OnAimStarted(AUnit& Unit, const UItemProfile& Profile)
{
	Honey = Cast<UHoneyBalloonProfile>(&Profile);
	if (!Honey)
	{
		return;
	}

	// 궤적은 던지는 본인만 본다(리슨 호스트 포함). 서버의 인스턴스도 조준 상태로 들어가지만
	// 그릴 것은 없다 — 확정이 올 때까지 기다리는 것이 서버의 몫이다.
	UWorld* const World = Unit.GetWorld();
	if (!Unit.IsLocallyControlled() || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	Preview = AItemAimPreview::Spawn(*World, &Unit, Honey->AimPreview);
	if (!Preview)
	{
		return;
	}

	UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
	Tick->OnTick.AddDynamic(this, &UGA_HoneyBalloon::HandleTick);
	Tick->ReadyForActivation();
}

void UGA_HoneyBalloon::HandleTick(float DeltaTime)
{
	const AUnit* const Unit = GetUnit();
	if (!Preview || !Unit || !Honey)
	{
		return;
	}

	const FVector Direction = UHoneyBalloonProfile::GetThrowDirection(*Unit);
	Preview->UpdateArc(Honey->GetThrowOrigin(*Unit, Direction), Honey->GetThrowVelocity(Direction),
		Honey->GravityScale, Honey->GetProjectileRadius(), Unit);
}

void UGA_HoneyBalloon::OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed)
{
	DestroyPreview();
}

void UGA_HoneyBalloon::DestroyPreview()
{
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}

void UGA_HoneyBalloon::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UHoneyBalloonProfile* const Balloon = Cast<UHoneyBalloonProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Balloon || !Balloon->ProjectileClass || !World || !IsAuthority())
	{
		return;
	}

	// 눈높이에서 시선 방향으로. 서버의 컨트롤 회전은 무브마다 갱신되므로 소유자가 본 것과 같다.
	const FVector Direction = UHoneyBalloonProfile::GetThrowDirection(Unit);
	const FVector Origin = Balloon->GetThrowOrigin(Unit, Direction);
	const FTransform SpawnTransform(Direction.Rotation(), Origin);

	AHoneyBalloonProjectile* const Thrown = World->SpawnActorDeferred<AHoneyBalloonProjectile>(
		Balloon->ProjectileClass, SpawnTransform, &Unit, &Unit, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Thrown)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 꿀풍선을 스폰하지 못했다."), *GetNameSafe(&Unit));
		return;
	}
	Thrown->Init(&Unit, Unit.GetTeam(), Balloon->GetThrowVelocity(Direction));
	Thrown->SetProfile(Balloon);
	Thrown->FinishSpawning(SpawnTransform);
}
