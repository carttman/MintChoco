#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplatLog.h"

#include "GameGameState.generated.h"

/**
 * 게임 맵의 GameState. 페인트에서 서버가 권한을 가진 두 가지를 모든 머신에 나른다.
 *
 * 하나는 스플랫 로그. 서버가 확정한 스플랫이 순서대로 쌓이고, 각 클라이언트는 아직
 * 안 그린 항목을 자기 표면에 그린다. 늦게 들어온 클라이언트도 같은 로그를 받아
 * 처음부터 재생하므로 별도의 동기화 경로가 없다.
 *
 * 다른 하나는 커버리지. 점수는 서버의 셀 그리드가 진실이고, 클라이언트 그리드는
 * 디버그 표시용일 뿐이다. 서버가 주기적으로 여기에 써 넣고 HUD는 이 값을 읽는다.
 */
UCLASS()
class MINTCHOCO_API AGameGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 스플랫을 로그에 남기고 서버 자신의 표면에도 바로 그린다. */
	void AddSplat(const FPaintSplat& Splat);

	/** 서버 전용. 로그를 비우고 모든 표면을 지운다. 클라이언트는 로그가 줄어든 것을 보고 따라 지운다. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/** 서버 그리드에서 온 월드 커버리지. 클라이언트에서는 마지막으로 복제된 값. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	const FPaintCoverage& GetWorldCoverage() const { return WorldCoverage; }

	/** 서버 전용. 지금 이 순간의 커버리지를 다시 잰다. 승패 판정 직전처럼 최신값이 필요할 때 쓴다. */
	void RefreshCoverage();

	/** 서버 전용. 경기가 끝나는 서버 월드 시각을 정한다. */
	void SetMatchEndTime(double InServerTime);

	/** 서버 전용. 승팀을 확정한다. Teams::None은 무승부. */
	void SetMatchResult(int32 InWinningTeam);

	/**
	 * 남은 시간(초). 종료 시각에서 서버 시간을 뺀 값이라 모든 머신이 같은 답을 낸다.
	 * 매 프레임 불러도 되고, 복제는 경기 시작 때 종료 시각 한 번뿐이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchEnded() const { return bMatchEnded; }

	/** 승팀. 경기 중이거나 무승부면 Teams::None. 둘의 구분은 IsMatchEnded로 한다. */
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetWinningTeam() const { return WinningTeam; }

	/** 경기가 끝났을 때 서버와 모든 클라이언트에서 한 번씩 불린다. 결과 UI를 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Match")
	void BP_OnMatchEnded(int32 InWinningTeam);

protected:
	/** 커버리지를 다시 재서 복제하는 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Paint", meta = (ClampMin = "0.05"))
	float CoverageRefreshInterval = 0.2f;

	UPROPERTY(ReplicatedUsing = OnRep_SplatLog)
	FPaintSplatLog SplatLog;

	UPROPERTY(Replicated)
	FPaintCoverage WorldCoverage;

	/**
	 * 경기가 끝나는 서버 월드 시각. 남은 초를 매초 복제하면 트래픽도 늘고 클라이언트마다
	 * 눈금이 튀므로, 시각 하나만 나르고 남은 시간은 각자 계산한다.
	 */
	UPROPERTY(Replicated)
	double MatchEndServerTime = 0.0;

	UPROPERTY(Replicated)
	int32 WinningTeam = INDEX_NONE;

	/** WinningTeam과 같은 프레임에 복제되므로 RepNotify 안에서 승팀을 읽어도 된다. */
	UPROPERTY(ReplicatedUsing = OnRep_MatchEnded)
	bool bMatchEnded = false;

	UFUNCTION()
	void OnRep_SplatLog();

	UFUNCTION()
	void OnRep_MatchEnded();

private:
	/** 클라이언트 전용. 로그에서 아직 안 그린 항목을 표면에 그린다. 표면이 생긴 뒤에만 부른다. */
	void ApplyNewSplats();

	void HandleMatchEnded();

	/** 로그 중 이미 표면에 그린 개수. 로그가 이보다 짧아졌다면 서버가 지운 것이다. */
	int32 AppliedSplatCount = 0;

	FTimerHandle CoverageTimer;
};
