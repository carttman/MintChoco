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
	 * 팀 번호를 인덱스로 쓰는 캐릭터 정의. [0]은 민트, [1]은 초코.
	 *
	 * 폰 클래스는 DefaultPawnClass 하나로 고정하고, 스폰된 유닛에 이 데이터를 주입해
	 * 메시와 애님을 가른다. 그래서 팀이 늘어도 폰 블루프린트는 하나만 관리하면 된다.
	 *
	 * TMap이 아니라 배열인 이유는 디테일 패널에서 + 만 누르면 되기 때문이다. 맵은
	 * 새 항목이 항상 기본 키(0)로 생성돼, 이미 0이 있으면 추가 자체가 거부된다.
	 * 팀 번호가 0부터 연속이라는 전제에 기대므로 비연속 팀이 생기면 맵으로 되돌려야 한다.
	 *
	 * 비어 있으면 폰 블루프린트의 UnitData 기본값이 그대로 쓰인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit")
	TArray<TObjectPtr<UUnitDataAsset>> TeamUnitData;

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

	/** 해당 팀의 캐릭터 정의. 설정되지 않았으면 nullptr. */
	UUnitDataAsset* FindUnitDataForTeam(int32 Team) const;

private:
	/** 스폰된 폰이 AUnit이면 팀에 맞는 캐릭터 정의를 넣는다. 아니면 경고를 남긴다. */
	void ApplyTeamUnitData(APawn* Pawn, const AController* NewPlayer) const;

	/** 맵의 모든 PlayerStart를 팀 전용 / 중립으로 나눈다. 팀 없는 기본 PlayerStart는 중립. */
	void GatherPlayerStarts(int32 Team, TArray<APlayerStart*>& OutTeamStarts, TArray<APlayerStart*>& OutNeutralStarts) const;

	/** 비어 있는 후보 중 적에게서 먼 쪽을 선호해 하나 고른다. 전부 막혔으면 nullptr. */
	APlayerStart* PickFreeStart(const TArray<APlayerStart*>& Candidates, int32 Team) const;

	bool IsStartOccupied(const APlayerStart* Start) const;

	/** 가장 가까운 적 폰까지의 거리. 적이 없으면 무한대로 친다. */
	float DistanceToNearestEnemy(const APlayerStart* Start, int32 Team) const;
};
