#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplatLog.h"

#include "GameGameState.generated.h"

/**
 * 한 판의 단계. 서버가 정하고 복제된다.
 * WaitingForPlayers(전원 준비 대기) → Countdown(3초) → Playing(타이머 진행) → Ended(결과).
 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	WaitingForPlayers	UMETA(DisplayName = "플레이어 대기"),
	Countdown			UMETA(DisplayName = "카운트다운"),
	Playing				UMETA(DisplayName = "경기 중"),
	Ended				UMETA(DisplayName = "종료"),
};

/**
 * 게임 맵의 GameState. 페인트에서 서버가 권한을 가진 두 가지를 모든 머신에 나른다.
 *
 * 경기 단계(EMatchPhase)도 여기서 나른다. 카운트다운과 경기 종료는 남은 초가 아니라 서버
 * 시각 하나를 복제하고 각 머신이 GetServerWorldTimeSeconds로 남은 시간을 계산한다.
 *
 * 하나는 스플랫 로그. 서버가 확정한 스플랫이 순서대로 쌓이고, 각 클라이언트는 아직
 * 안 그린 항목을 자기 표면에 그린다. 늦게 들어온 클라이언트도 같은 로그를 받아
 * 처음부터 재생하므로 별도의 동기화 경로가 없다. 표면이 간직하지 않는 방향에 맞은
 * 일시 스플랫은 로그에 남기지 않고 신뢰성 없는 멀티캐스트로만 보낸다: 잠깐 보이고
 * 사라지는 연출이라 늦게 들어온 클라이언트가 볼 이유가 없고, 로그가 그만큼 자라지도 않는다.
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

	//~ 경기 단계

	UFUNCTION(BlueprintPure, Category = "Match")
	EMatchPhase GetMatchPhase() const { return MatchPhase; }

	/** 타이머가 도는 중인지. 준비 대기와 카운트다운 동안은 false. */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchLive() const { return MatchPhase == EMatchPhase::Playing; }

	/** 카운트다운 단계에서 남은 초. 다른 단계면 0. */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetCountdownRemaining() const;

	/** 한 판의 길이(초). 경기 전에는 GetRemainingTime이 이 값을 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "Match")
	float GetMatchDuration() const { return MatchDuration; }

	/** 이 단계에서 플레이어가 움직이고 쏠 수 있는지. 대기·카운트다운 동안은 묶인다. 순수 함수라 테스트 대상. */
	static bool AllowsPlayerInput(EMatchPhase Phase);

	/** 월드의 GameState가 AGameGameState면 그 단계의 규칙, 아니면(샘플 맵) 항상 허용. */
	static bool IsPlayerInputAllowed(const UWorld* World);

	/** 서버 전용. 단계를 바꾼다. 같은 단계면 아무것도 하지 않는다. */
	void SetMatchPhase(EMatchPhase NewPhase);

	/** 서버 전용. 카운트다운이 끝나는 서버 월드 시각. */
	void SetCountdownEndTime(double InServerTime);

	/** 서버 전용. 한 판의 길이. 경기 시작 전 HUD가 보여줄 값이다. */
	void SetMatchDuration(float InSeconds);

	/** 단계가 바뀔 때 서버와 모든 클라이언트에서 한 번씩. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Match")
	void BP_OnMatchPhaseChanged(EMatchPhase NewPhase);

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

	/**
	 * RepNotify인 이유는 디버그 표시 때문이다. 클라이언트는 이 값이 갱신되는 시점을
	 * 알아야 자기 화면에 자기가 받은 수치를 그릴 수 있다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_WorldCoverage)
	FPaintCoverage WorldCoverage;

	/**
	 * 경기가 끝나는 서버 월드 시각. 남은 초를 매초 복제하면 트래픽도 늘고 클라이언트마다
	 * 눈금이 튀므로, 시각 하나만 나르고 남은 시간은 각자 계산한다.
	 */
	UPROPERTY(Replicated)
	double MatchEndServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EMatchPhase MatchPhase = EMatchPhase::WaitingForPlayers;

	/** 카운트다운이 끝나는(경기가 시작되는) 서버 월드 시각. */
	UPROPERTY(Replicated)
	double CountdownEndServerTime = 0.0;

	UPROPERTY(Replicated)
	float MatchDuration = 0.0f;

	UFUNCTION()
	void OnRep_MatchPhase();

	UPROPERTY(Replicated)
	int32 WinningTeam = INDEX_NONE;

	/** WinningTeam과 같은 프레임에 복제되므로 RepNotify 안에서 승팀을 읽어도 된다. */
	UPROPERTY(ReplicatedUsing = OnRep_MatchEnded)
	bool bMatchEnded = false;

	UFUNCTION()
	void OnRep_SplatLog();

	UFUNCTION()
	void OnRep_WorldCoverage();

	UFUNCTION()
	void OnRep_MatchEnded();

	/** 일시 스플랫 전용. 서버 포함 모든 머신이 받아서 연출만 띄운다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTransientSplat(const FPaintSplat& Splat);

private:
	/** 클라이언트 전용. 로그에서 아직 안 그린 항목을 표면에 그린다. 표면이 생긴 뒤에만 부른다. */
	void ApplyNewSplats();

	void HandleMatchEnded();

	/**
	 * 팀별 점유 면적을 화면에 띄운다. 콘솔 변수 mc.ShowCoverage 로 켠다.
	 *
	 * 서버와 클라이언트가 각자 자기가 가진 값을 그리므로, 계산이 틀린 것인지 표시가
	 * 틀린 것인지 두 화면을 비교해 바로 가릴 수 있다.
	 */
	void DrawCoverageDebug() const;

	/** 로그 중 이미 표면에 그린 개수. 로그가 이보다 짧아졌다면 서버가 지운 것이다. */
	int32 AppliedSplatCount = 0;

	FTimerHandle CoverageTimer;
};
