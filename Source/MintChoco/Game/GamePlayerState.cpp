// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/GamePlayerState.h"

#include "Net/UnrealNetwork.h"

void AGamePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGamePlayerState, Nickname);
	DOREPLIFETIME(AGamePlayerState, Team);
}
