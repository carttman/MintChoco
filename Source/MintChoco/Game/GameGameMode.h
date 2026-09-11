// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/TeamTypes.h"
#include "GameGameMode.generated.h"

class AItemSpawnPoint;
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

	/** GameModeBase에는 HandleMatchHasStarted가 없다. 경기 시작 훅은 여기다. */
	virtual void StartPlay() override;

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

	/**
	 * 비어 있는 지점 중 하나를 무작위로 고른다. bFree[i]가 i번 지점의 상태. 전부 차 있으면
	 * INDEX_NONE. 순수 함수라 테스트가 액터 없이 검사한다.
	 */
	static int32 PickFreeSpawnIndex(const TArray<bool>& bFree, const FRandomStream& Random);

	/** 모든 플레이어가 준비됐는지. 플레이어가 없으면 false. 순수 함수라 테스트가 액터 없이 검사한다. */
	static bool AreAllReady(const TArray<bool>& bReady);

protected:
	/** 아이템이 나오는 주기(초). 0 이하면 아이템이 나오지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ItemSpawnInterval = 15.0f;

	/** 아이템이 나오기 몇 초 전에 그 자리에 레이저를 세울지. 주기보다 길면 주기로 잘린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ItemSpawnWarning = 5.0f;
	/** 한 판의 길이(초). 0 이하로 두면 타이머를 걸지 않아 경기가 끝나지 않는다(디버그용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.0"))
	float MatchDuration = 90.0f;

	/** 전원이 준비된 뒤 경기 시작까지의 카운트다운(초). 0이면 바로 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float CountdownDuration = 3.0f;

	/** 전원 준비를 기다리는 상한(초). 넘기면 준비되지 않은 플레이어가 있어도 카운트다운을 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ReadyTimeout = 20.0f;

	/** 준비 상태를 다시 보는 주기(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ReadyCheckInterval = 0.25f;

	/**
	 * 1위와 2위의 상대 격차가 이 값 이하면 무승부로 친다. 0.1 = 두 팀이 칠한 양의 10% 차이.
	 *
	 * 맵 전체 면적이 아니라 두 팀이 칠한 양의 합으로 나눈다. 맵의 대부분이 비어 있어도
	 * 접전인지 압승인지가 그대로 드러나고, 나중에 사격 속도나 스플랫 크기를 올려
	 * 도포량이 통째로 늘어도 이 값을 다시 손볼 필요가 없다.
	 *
	 * 절대 점유율로 비교하면 그때마다 기준을 옮겨야 한다. 지금은 0.17%가 "많이 칠한"
	 * 수준이지만 도포량이 늘면 그 값은 의미를 잃는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrawMarginFraction = 0.1f;


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
	/** 모든 PlayerState가 준비됐거나 ReadyTimeout이 지났으면 카운트다운으로 넘어간다. */
	void CheckPlayersReady();

	/** 단계 Countdown. CountdownDuration 뒤 StartMatch. */
	void StartCountdown();

	/** 단계 Playing. 아이템 스폰과 경기 타이머가 여기서 시작된다. */
	void StartMatch();

	/** 시간이 다 됐을 때. 커버리지를 다시 재고 더 많이 칠한 팀을 승팀으로 확정한다. 같으면 무승부. */
	void OnMatchTimeExpired();

	FTimerHandle ReadyCheckTimer;
	FTimerHandle CountdownTimer;
	FTimerHandle MatchTimer;

	/** 준비 대기를 시작한 서버 시각. ReadyTimeout의 기준. */
	double WaitStartTime = 0.0;

	/** 스폰된 폰이 AUnit이면 팀에 맞는 캐릭터 정의를 넣는다. 아니면 경고를 남긴다. */
	void ApplyTeamUnitData(APawn* Pawn, const AController* NewPlayer) const;

	/** 맵의 모든 PlayerStart를 팀 전용 / 중립으로 나눈다. 팀 없는 기본 PlayerStart는 중립. */
	void GatherPlayerStarts(int32 Team, TArray<APlayerStart*>& OutTeamStarts, TArray<APlayerStart*>& OutNeutralStarts) const;

	/** 비어 있는 후보 중 적에게서 먼 쪽을 선호해 하나 고른다. 전부 막혔으면 nullptr. */
	APlayerStart* PickFreeStart(const TArray<APlayerStart*>& Candidates, int32 Team) const;

	bool IsStartOccupied(const APlayerStart* Start) const;

	/** 가장 가까운 적 폰까지의 거리. 적이 없으면 무한대로 친다. */
	float DistanceToNearestEnemy(const APlayerStart* Start, int32 Team) const;

	/** 맵의 스폰 지점을 모으고 주기 타이머를 건다. 지점이나 아이템 목록이 비면 아무것도 걸지 않는다. */
	void StartItemSpawning();

	/** 빈 지점 하나에 무작위 아이템을 예고 상태로 놓는다. 전부 차 있으면 이번 주기는 건너뛴다. */
	void SpawnNextItem();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AItemSpawnPoint>> ItemSpawnPoints;

	FTimerHandle ItemSpawnTimer;
	FRandomStream ItemRandom;
};
