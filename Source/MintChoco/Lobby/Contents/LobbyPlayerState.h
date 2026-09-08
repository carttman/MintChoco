// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "LobbyPlayerState.generated.h"

/**
 *
 */
UCLASS()
class MINTCHOCO_API ALobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual void ClientInitialize(AController* C) override;

public:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Ready();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Team(int32 TeamId);
public:
	UFUNCTION(BlueprintImplementableEvent)
	void BP_RefreshLobbyUI();

	UFUNCTION(BlueprintCallable)
	void RefreshLobbyUI();

private:
	UFUNCTION()
	void OnRep_NicknameChange();

	UFUNCTION(BlueprintCallable)
	void SetNickname();

	UFUNCTION(Server, Reliable)
	void Server_SetNickname(const FText& NewNickname);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	bool Ready;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_NicknameChange)
	FText Nickname;

	/**
	 * 기본값은 Teams::None이어야 한다. 0으로 두면 팀을 고르지 않은 플레이어가 민트와
	 * 같은 값이 되어, 게임 맵으로 복사된 뒤 조용히 민트로 스폰된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	int32 Team = Teams::None;
};
