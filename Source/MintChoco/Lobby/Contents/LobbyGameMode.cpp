// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"

#include "Lobby/Contents/LobbyPlayerState.h"
#include "GameFramework/PlayerController.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ALobbyPlayerState* LobbyPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ALobbyPlayerState>() : nullptr;
	if (nullptr == LobbyPlayerState)
	{
		return;
	}

	// 접속 URL의 Name 옵션은 엔진이 알아서 채운다. ULocalPlayer::SpawnPlayActor가 플랫폼
	// (스팀) 닉네임을 실어 보내고, AGameModeBase::InitNewPlayer가 그것을 PlayerState의
	// PlayerName에 넣는다. 여기는 그 뒤라서 값이 이미 들어 있다.
	//
	// 이 시점은 PlayerState가 처음 복제되기 전이므로, 지금 채워두면 이름이 PlayerState와
	// 같은 번들로 도착한다. 클라이언트의 Server_SetNickname RPC를 기다리면 왕복이 한 번
	// 더 들어가 그만큼 늦게 보인다.
	if (LobbyPlayerState->Nickname.IsEmpty())
	{
		LobbyPlayerState->Nickname = FText::FromString(LobbyPlayerState->GetPlayerName());
	}

	// OnRep_NicknameChange는 서버에서 호출되지 않으므로, 리슨 호스트의 목록은 직접 갱신한다.
	LobbyPlayerState->RefreshLobbyUI();
}

void ALobbyGameMode::TryStartGame()
{
	BP_TryStartGame();
}
