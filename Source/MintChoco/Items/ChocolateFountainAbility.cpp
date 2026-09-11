#include "Items/ChocolateFountainAbility.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/ChocolateFountain.h"
#include "Items/ChocolateFountainProfile.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"

void UGA_ChocolateFountain::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UChocolateFountainProfile* const Fountain = Cast<UChocolateFountainProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Fountain || !Fountain->DomeClass || !World || !IsAuthority())
	{
		return;
	}

	// 캡슐 바닥. 반구의 중심이 발밑이라 절반은 땅에 묻힌다.
	const float HalfHeight = Unit.GetCapsuleComponent() ? Unit.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FTransform SpawnTransform(FRotator::ZeroRotator, Unit.GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight));

	AChocolateFountain* const Dome = World->SpawnActorDeferred<AChocolateFountain>(
		Fountain->DomeClass, SpawnTransform, &Unit, &Unit, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Dome)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 초콜릿 분수를 스폰하지 못했다."), *GetNameSafe(&Unit));
		return;
	}
	Dome->Init(Unit.GetTeam(), GetPaintId(), Fountain->Radius, Fountain->Lifetime);
	Dome->FinishSpawning(SpawnTransform);

	// 돔이 덮는 만큼 발밑을 칠한다. 사용자 팀 색이라 돔 벽에 삼켜지지 않고 그대로 통과한다.
	APaintBurst::Spawn(*World, SpawnTransform.GetLocation(), Fountain->MakeBurst(GetPaintId(), FMath::Rand()));
}
