// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

/**
 *
 */
UCLASS()
class MINTCHOCO_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * PlayerState가 처음 복제되기 전에 닉네임을 채워 넣는다.
	 *
	 * 클라이언트가 RPC로 알려주기를 기다리면 왕복이 한 번 더 들어가, 이름이 빈 칸으로
	 * 먼저 보였다가 뒤늦게 채워진다.
	 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_TryStartGame();

	UFUNCTION(BlueprintCallable)
	void TryStartGame();
};
