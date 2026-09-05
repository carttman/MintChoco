// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GamePlayerState.generated.h"

/**
 *
 */
UCLASS()
class MINTCHOCO_API AGamePlayerState : public APlayerState
{
	GENERATED_BODY()

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

public:
	/**
	 * 로비에서 정해져 ReceiveCopyProperties로 넘어온 팀. Teams::Mint 또는 Teams::Choco.
	 * Team이 protected라 게임모드가 직접 읽을 수 없어 여기를 통한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Team")
	FText GetNickname() const { return Nickname; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	int32 Team;
};
