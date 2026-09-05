// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/TeamTypes.h"
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

	/**
	 * 기본값은 반드시 Teams::None이어야 한다. 0으로 두면 "팀이 정해지지 않음"과
	 * "민트"가 같은 값이 되어, 로비에서 팀을 고르지 않았거나 ReceiveCopyProperties가
	 * 값을 옮기지 못한 플레이어가 조용히 민트로 스폰된다. Teams::IsValidId(0)이
	 * true라 게임모드의 경고도 걸리지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Team)
	int32 Team = Teams::None;

	/**
	 * 팀은 폰이 아니라 이 액터에 실려 오므로, 폰의 OnRep_PlayerState와 도착 순서가
	 * 보장되지 않는다. 팀 값이 나중에 도착하는 경우 여기서 무기 색을 다시 맞춘다.
	 */
	UFUNCTION()
	void OnRep_Team();
};
