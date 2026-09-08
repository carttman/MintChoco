// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "Game/TeamTypes.h"
#include "TeamPlayerStart.generated.h"

/**
 * 팀이 지정된 스폰 지점.
 *
 * 엔진에도 APlayerStart::PlayerStartTag가 있지만 FName 자유 입력이라 오타가
 * 런타임까지 안 잡힌다. 맵에 수십 개를 배치할 물건이므로 타입이 있는 필드를 쓴다.
 *
 * 팀이 지정되지 않은 평범한 APlayerStart는 게임모드가 중립 지점으로 취급하므로,
 * 이 액터를 하나도 배치하지 않은 맵도 그대로 동작한다.
 */
UCLASS()
class MINTCHOCO_API ATeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	/** 이 지점을 쓸 팀. Teams::None이면 어느 팀이든 쓸 수 있는 중립 지점이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Team")
	int32 Team = Teams::None;
};
