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

protected:
	/** 커버리지를 다시 재서 복제하는 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Paint", meta = (ClampMin = "0.05"))
	float CoverageRefreshInterval = 0.2f;

	UPROPERTY(ReplicatedUsing = OnRep_SplatLog)
	FPaintSplatLog SplatLog;

	UPROPERTY(Replicated)
	FPaintCoverage WorldCoverage;

	UFUNCTION()
	void OnRep_SplatLog();

private:
	/** 클라이언트 전용. 로그에서 아직 안 그린 항목을 표면에 그린다. 표면이 생긴 뒤에만 부른다. */
	void ApplyNewSplats();

	void RefreshCoverage();

	/** 로그 중 이미 표면에 그린 개수. 로그가 이보다 짧아졌다면 서버가 지운 것이다. */
	int32 AppliedSplatCount = 0;

	FTimerHandle CoverageTimer;
};
