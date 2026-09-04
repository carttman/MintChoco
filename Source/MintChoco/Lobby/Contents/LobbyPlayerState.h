// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	int32 Team;
};
