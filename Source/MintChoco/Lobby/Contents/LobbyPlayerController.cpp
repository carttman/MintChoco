// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyPlayerController.h"
#include "LobbyPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "LobbyGameMode.h"
#include "MintChoco.h"

// PlayerState는 컨트롤러에서 직접 읽는다. 폰을 거치면(GetPawn()->GetPlayerState()) Seamless
// Travel 직후처럼 폰이 아직 붙지 않은 순간에 서버가 널 참조로 죽거나, 버튼이 조용히 무시된다.
void ALobbyPlayerController::Server_HandleReadyButton_Implementation()
{
	if (HasAuthority() == false)
		return;

	ALobbyPlayerState* LobbyPlayerState = GetPlayerState<ALobbyPlayerState>();
	if (LobbyPlayerState == nullptr)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 로비 PlayerState가 없어 준비를 무시했다."), *GetNameSafe(this));
		return;
	}

	// 팀을 고르지 않은 플레이어의 준비는 받지 않는다. UI가 버튼을 잠그지만 서버가 진실이다.
	if (!Teams::IsValidId(LobbyPlayerState->Team))
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 팀을 고르지 않은 채 준비를 눌러 무시했다."), *GetNameSafe(LobbyPlayerState));
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

	ALobbyPlayerState* LobbyPlayerState = GetPlayerState<ALobbyPlayerState>();
	if (LobbyPlayerState == nullptr)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 로비 PlayerState가 없어 팀 선택(%d)을 무시했다."), *GetNameSafe(this), TeamId);
		return;
	}

	LobbyPlayerState->Multicast_Team(TeamId);
}
