// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/TeamTypes.h"
#include "GameGameMode.generated.h"

class APlayerStart;
class UUnitDataAsset;

/**
 * 게임 맵의 게임모드. 로비에서 정해진 팀을 받아 스폰 지점과 캐릭터를 결정한다.
 *
 * 로비는 Seamless Travel로 넘어오고, 팀은 그 과정에서 PlayerState의
 * CopyProperties(BP의 ReceiveCopyProperties)로 옮겨진다. 엔진의
 * HandleSeamlessTravelPlayer는 그 복사를 끝낸 뒤에 ChoosePlayerStart와 폰 스폰을
 * 부르므로, 이 클래스의 모든 훅에서 PlayerState->GetTeam()을 믿어도 된다.
 */
UCLASS()
class MINTCHOCO_API AGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameGameMode();

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/**
	 * 기본 구현은 Player->StartSpot이 있으면 그걸 그대로 재사용하고 ChoosePlayerStart를
	 * 부르지 않는다. Seamless Travel의 InitSeamlessTravelPlayer가 StartSpot을 이미
	 * 채워두기 때문에, 그대로 두면 죽을 때마다 정확히 같은 지점에서 부활한다.
	 */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }

	/** 컨트롤러의 팀. PlayerState가 아직 없으면 TeamNone. */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeamOf(const AController* Player) const;

protected:
	/**
	 * 팀별로 스폰할 캐릭터. 비워두면 폰 블루프린트의 UnitData 기본값이 그대로 쓰인다.
	 *
	 * 팀과 캐릭터의 대응이 여기 한 곳에만 있으므로, 나중에 로비에서 캐릭터를 직접
	 * 고르게 되면 이 조회를 PlayerState의 선택값으로 바꾸는 것으로 끝난다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit")
	TMap<int32, TObjectPtr<UUnitDataAsset>> TeamUnitData;

	/**
	 * 스폰 지점이 비었는지 검사할 때 쓰는 캡슐 크기. 유닛의 캡슐과 같게 두면 된다.
	 * PlayerStart 자신의 캡슐은 충돌이 꺼져 있어 그대로 쓸 수 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	float SpawnClearanceRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	float SpawnClearanceHalfHeight = 88.0f;

	/**
	 * 점수 상위 몇 개 중에서 무작위로 고를지.
	 *
	 * 1로 두면 항상 최적을 고르게 되는데, 그러면 결정론적이라 리스폰 지점이 다시
	 * 한 곳으로 고정된다. 적에게서 먼 쪽을 선호하되 예측은 안 되게 하는 값이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "1"))
	int32 SpawnCandidatePoolSize = 3;

	UUnitDataAsset* FindUnitDataForTeam(int32 Team) const;

private:
	/** 맵의 모든 PlayerStart를 팀 전용 / 중립으로 나눈다. 팀 없는 기본 PlayerStart는 중립. */
	void GatherPlayerStarts(int32 Team, TArray<APlayerStart*>& OutTeamStarts, TArray<APlayerStart*>& OutNeutralStarts) const;

	/** 비어 있는 후보 중 적에게서 먼 쪽을 선호해 하나 고른다. 전부 막혔으면 nullptr. */
	APlayerStart* PickFreeStart(const TArray<APlayerStart*>& Candidates, int32 Team) const;

	bool IsStartOccupied(const APlayerStart* Start) const;

	/** 가장 가까운 적 폰까지의 거리. 적이 없으면 무한대로 친다. */
	float DistanceToNearestEnemy(const APlayerStart* Start, int32 Team) const;
};
