// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/GamePlayerState.h"

#include "Game/Unit.h"
#include "Net/UnrealNetwork.h"

void AGamePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGamePlayerState, Nickname);
	DOREPLIFETIME(AGamePlayerState, Team);
}

void AGamePlayerState::OnRep_Team()
{
	// 폰이 아직 붙지 않았으면 뒤이어 AUnit::OnRep_PlayerState가 같은 일을 한다.
	// 둘 중 무엇이 먼저 도착하든 한 번은 반영되도록 양쪽에 걸어 둔다.
	if (AUnit* Unit = Cast<AUnit>(GetPawn()))
	{
		Unit->ApplyTeamToWeapon();
	}
}
