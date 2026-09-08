// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyPlayerController.h"
#include "LobbyPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "LobbyGameMode.h"

void ALobbyPlayerController::Server_HandleReadyButton_Implementation()
{
	if (HasAuthority() == false)
		return;

	ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(GetPawn()->GetPlayerState());
	if (LobbyPlayerState == nullptr)
		return;

	// 팀을 고르지 않은 플레이어의 준비는 받지 않는다. UI가 버튼을 잠그지만 서버가 진실이다.
	if (!Teams::IsValidId(LobbyPlayerState->Team))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 팀을 고르지 않은 채 준비를 눌러 무시했다."), *GetNameSafe(LobbyPlayerState));
		return;
	}

	LobbyPlayerState->Multicast_Ready();

	ALobbyGameMode* LobbyGameMode = Cast<ALobbyGameMode>(UGameplayStatics::GetGameMode(this));
	if (LobbyGameMode)
		LobbyGameMode->TryStartGame();
}

void ALobbyPlayerController::Server_HandleTeamButton_Implementation(int32 TeamId)
{
	if (HasAuthority() == false)
		return;

	ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(GetPawn()->GetPlayerState());
	if (LobbyPlayerState)
		LobbyPlayerState->Multicast_Team(TeamId);
}

