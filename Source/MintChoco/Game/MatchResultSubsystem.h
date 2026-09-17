#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Game/MatchResult.h"

#include "MatchResultSubsystem.generated.h"

class AMatchResultStage;
class APlayerController;
class UInputComponent;
class UMatchResultConfettiWidget;
class UMatchResultFrameWidget;
class UMatchResultBarWidget;

/**
 * 경기가 끝난 뒤의 결과 연출을 이 머신에서 돌리는 감독.
 *
 * AGameGameState::HandleMatchEnded 가 서버·클라이언트 가리지 않고 한 번씩 부른다. 연출에 필요한
 * 값(승팀, 최종 커버리지)은 이미 복제되므로 여기서 주고받는 것은 없다: 각 머신이 같은 입력으로
 * 같은 그림을 로컬에서 만든다. 화면을 실제로 넘기는 것은 서버의 로비 복귀 타이머 하나뿐이고,
 * 그 시간은 AGameGameMode 가 FMatchResultTimeline::GetTotalSeconds 에서 가져간다.
 *
 * 단계 진행은 월드 타이머 사슬이다(AGameGameMode 의 경기 단계와 같은 방식). 매 프레임 움직이는
 * 것은 무대 액터 자신의 틱이 맡는다.
 *
 * 최종 커버리지는 연출이 시작될 때가 아니라 BarReal 단계에 들어갈 때 읽는다. 경기 종료 직전의
 * 마지막 복제가 도착할 시간을 벌기 위해서다.
 */
UCLASS()
class MINTCHOCO_API UMatchResultSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트의 서브시스템. 게임 월드가 아니면 nullptr. */
	static UMatchResultSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** 끝난 경기의 결과로 연출을 시작한다. 이미 돌고 있으면 아무것도 하지 않는다. */
	void BeginSequence();

	/** 같은 연출을 지어낸 값으로 돌린다. mc.Result.Preview 가 쓴다. 비율은 0~1. */
	void BeginPreview(float LeftCoverage, float RightCoverage, int32 WinningTeam);

	/** 연출을 중간에 끊고 화면을 경기 상태로 되돌린다. */
	void Abort();

	/**
	 * 연출을 건너뛰고 이 머신만 로비로 떠난다. 건너뛰기 키가 부른다.
	 *
	 * 남의 머신은 아무것도 모른다. 연출은 그쪽에서 끝까지 돌고, 서버의 로비 복귀도 예정대로다.
	 */
	void Skip();

	bool IsRunning() const { return Phase != EMatchResultPhase::Idle; }

	EMatchResultPhase GetPhase() const { return Phase; }

	/** 연출 전체 길이(초). 서버가 로비 복귀를 이만큼 뒤로 미룬다. */
	static float GetTotalSeconds();

private:
	/** 이 머신에서 화면을 보고 있는 컨트롤러. 데디케이티드 서버에는 없다. */
	APlayerController* FindLocalController() const;

	/** 설정에서 시간 값을 읽어 시계를 만든다. 가림막 길이는 UScreenFadeSettings 에서 온다. */
	static FMatchResultTimeline MakeTimeline();

	void Start(const FMatchResultInput& InResult, bool bInPreview);

	/** 단계에 들어가 그 단계의 일을 하고, 다음 단계를 예약한다. */
	void EnterPhase(EMatchResultPhase NewPhase);

	/** 미리보기의 마지막. 경기라면 서버가 로비로 보낼 순간이라, 여기서는 연출을 걷는다. */
	void FinishPreview();

	/** 화면을 무대로 갈아치운다. 가림막이 완전히 덮여 있는 동안에만 부른다. */
	void TakeOverView();

	/** 경기 화면으로 되돌린다. */
	void ReleaseView();

	/** 맵에 놓인 무대를 찾고, 없으면 페인트 영역에서 구도를 잡아 하나 스폰한다. */
	AMatchResultStage* FindOrSpawnStage();

	/** 칠할 수 있는 액터 전체를 감싸는 상자. 무대가 놓이지 않은 맵에서만 쓴다. */
	FBox MeasurePaintBounds() const;

	/** 화면 아래에 결과 바를 만든다. 경기 중 HUD 의 바와는 별개의 인스턴스다. */
	void CreateBar();

	/** 화면을 두르는 장식 테두리를 투명한 채로 얹는다. 바보다 아래에 깔아 바를 가리지 않는다. */
	void CreateFrame();

	/** 화면 위에서 쏟아질 스티커를 얹는다. 실제로 뿌리는 것은 Hold 에 들어가는 순간이다. */
	void CreateConfetti();

	/**
	 * 건너뛰기 키를 컨트롤러에 올린다. 컨트롤러가 들고 있는 입력 더미를 건드리지 않고 우리 것을
	 * 하나 밀어 넣는다: 끝날 때 통째로 빼면 되므로 남의 바인딩 사이에서 우리 것만 골라낼 일이 없다.
	 */
	void BindSkipKeys();
	void UnbindSkipKeys();

	/** 이번 단계의 값을 바에 밀어 넣는다. */
	void PushBar();

	/** 경기 중 HUD 와 모든 유닛을 이 머신에서만 숨기거나 되돌린다. */
	void SetMatchVisualsHidden(bool bHidden);

	UPROPERTY(Transient)
	TObjectPtr<AMatchResultStage> Stage;

	UPROPERTY(Transient)
	TObjectPtr<UMatchResultBarWidget> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UMatchResultFrameWidget> Frame;

	UPROPERTY(Transient)
	TObjectPtr<UMatchResultConfettiWidget> Confetti;

	/** 건너뛰기 키만 담은 입력 더미. 연출이 도는 동안만 컨트롤러 위에 얹혀 있다. */
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> SkipInput;

	/** 무대가 맵에 없어서 직접 스폰했는지. 끝낼 때 치워야 한다. */
	bool bSpawnedStage = false;

	FMatchResultTimeline Timeline;
	FMatchResultInput Result;
	EMatchResultPhase Phase = EMatchResultPhase::Idle;

	/** 지어낸 값으로 도는 중인지. 그러면 BarReal 에서 실제 커버리지를 읽지 않는다. */
	bool bPreview = false;

	/** 건너뛰기를 이미 눌렀다. 페이드가 내려오는 동안 또 눌러도 한 번만 떠난다. */
	bool bSkipped = false;

	FTimerHandle PhaseTimer;
};
