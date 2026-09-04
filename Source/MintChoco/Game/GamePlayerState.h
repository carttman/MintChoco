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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	int32 Team;
};
