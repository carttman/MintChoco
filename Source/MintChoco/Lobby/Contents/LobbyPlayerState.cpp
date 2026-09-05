// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyPlayerState.h"

#include "OnlineSessionsSubsystem.h"
#include "Net/UnrealNetwork.h"

void ALobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyPlayerState, Ready);
	DOREPLIFETIME(ALobbyPlayerState, Nickname);

	// Replicated로 선언해 놓고 여기에 빠져 있으면, Net.AutoRegisterReplicatedProperties가
	// 기본값(true)인 동안에만 우연히 동작한다. 그 cvar를 끄는 순간 COND_Never가 되어
	// 경고 한 줄 없이 복제가 멈춘다.
	DOREPLIFETIME(ALobbyPlayerState, Team);
}

void ALobbyPlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	// 원격 클라이언트의 경우, 이 이벤트가 발생한 시점에야 비로소 Owner(및 GetPlayerController())가 유효해집니다.
	// BeginPlay에서 호출되는 SetNickname()은 이보다 먼저 실행되지만, IsLocalController()를 아직 확인할 수 없기 때문에
	// 별다른 동작 없이(silent no-op) 그냥 넘어가게 됩니다.
	SetNickname();
}

void ALobbyPlayerState::Multicast_Team_Implementation(int32 TeamId)
{
	Team = TeamId;

	RefreshLobbyUI();
}

void ALobbyPlayerState::Multicast_Ready_Implementation()
{
	Ready = true;

	RefreshLobbyUI();
}

void ALobbyPlayerState::RefreshLobbyUI()
{
	BP_RefreshLobbyUI();
}

void ALobbyPlayerState::OnRep_NicknameChange()
{
	RefreshLobbyUI();
}

void ALobbyPlayerState::SetNickname()
{
	// 해당 클라이언트만이 자신의 로컬 OnlineSubsystem 닉네임을 알고 있다.
	// 가드가 없으면 모든 플레이어의 닉네임이 동일한 값으로 복제(replicate)된다.
	const APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	auto* OSS = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();
	Server_SetNickname(OSS->GetLocalPlayerNicknameToFText());
}

void ALobbyPlayerState::Server_SetNickname_Implementation(const FText& NewNickname)
{
	Nickname = NewNickname;


	// OnRep_NicknameChange는 변경을 수행한 권한 주체(authority)에서는 호출되지 않으므로,
	// 서버나 리슨 호스트의 UI는 여기서 명시적으로 갱신해야 한다. 반면 원격
	// 클라이언트는 여전히 OnRep_NicknameChange를 통해 정상적으로 변경 사항을 반영한다.
	RefreshLobbyUI();
}
