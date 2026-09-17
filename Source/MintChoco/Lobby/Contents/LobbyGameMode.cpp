// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"

#include "Lobby/Contents/LobbyPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "MintChoco.h"
#include "GameFramework/GameStateBase.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	InitLobbyPlayer(NewPlayer, /*bReturningFromMatch=*/false);
}

void ALobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	// Super가 새 PlayerController를 스폰하고 옛 PlayerState의 CopyProperties로 값을 옮긴 뒤
	// GenericPlayerInitialization(HUD)과 HandleStartingNewPlayer(폰)를 부른다. PostLogin은 오지 않는다.
	Super::HandleSeamlessTravelPlayer(C);

	InitLobbyPlayer(Cast<APlayerController>(C), /*bReturningFromMatch=*/true);
}

void ALobbyGameMode::InitLobbyPlayer(APlayerController* Player, bool bReturningFromMatch)
{
	ALobbyPlayerState* LobbyPlayerState = Player ? Player->GetPlayerState<ALobbyPlayerState>() : nullptr;
	if (nullptr == LobbyPlayerState)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 로비 PlayerState가 없어 초기화하지 못했다(복귀=%d)."),
			*GetNameSafe(Player), bReturningFromMatch ? 1 : 0);
		return;
	}

	// 접속 URL의 Name 옵션은 엔진이 알아서 채운다. ULocalPlayer::SpawnPlayActor가 플랫폼
	// (스팀) 닉네임을 실어 보내고, AGameModeBase::InitNewPlayer가 그것을 PlayerState의
	// PlayerName에 넣는다. 여기는 그 뒤라서 값이 이미 들어 있다.
	//
	// 처음 입장은 PlayerState가 처음 복제되기 전이므로, 지금 채워두면 이름이 PlayerState와
	// 같은 번들로 도착한다. 클라이언트의 Server_SetNickname RPC를 기다리면 왕복이 한 번
	// 더 들어가 그만큼 늦게 보인다. 복귀는 AGamePlayerState::CopyProperties가 닉네임을
	// 옮겨 두므로 보통 이미 차 있다.
	if (LobbyPlayerState->Nickname.IsEmpty())
	{
		LobbyPlayerState->Nickname = FText::FromString(LobbyPlayerState->GetPlayerName());
	}

	// OnRep_NicknameChange는 서버에서 호출되지 않으므로, 리슨 호스트의 목록은 직접 갱신한다.
	LobbyPlayerState->RefreshLobbyUI();

	BP_OnPlayerJoinedLobby(Player, bReturningFromMatch);
}

void ALobbyGameMode::TryStartGame()
{
	BP_TryStartGame();
}

bool ALobbyGameMode::AreAllReady(const TArray<const APlayerState*>& PlayerStates, int32 MinPlayers)
{
	int32 Counted = 0;
	for (const APlayerState* const PlayerState : PlayerStates)
	{
		const ALobbyPlayerState* const Lobby = Cast<ALobbyPlayerState>(PlayerState);
		// 게임 맵에서 따라온 옛 PlayerState(엔진이 곧 지운다)나 관전자는 준비 판정에서 뺀다.
		// 머릿수에도 들어가지 않으므로, 관전자를 데려와 최소 인원을 채울 수는 없다.
		if (!Lobby || Lobby->IsInactive() || Lobby->IsSpectator())
		{
			continue;
		}
		if (!Lobby->Ready)
		{
			return false;
		}
		++Counted;
	}
	return Counted >= FMath::Max(MinPlayers, 1);
}

bool ALobbyGameMode::AreAllPlayersReady() const
{
	if (!GameState)
	{
		return false;
	}

	TArray<const APlayerState*> PlayerStates;
	PlayerStates.Reserve(GameState->PlayerArray.Num());
	for (const APlayerState* const PlayerState : GameState->PlayerArray)
	{
		PlayerStates.Add(PlayerState);
	}
	return AreAllReady(PlayerStates, MinPlayersToStart);
}
