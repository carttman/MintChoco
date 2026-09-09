#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ScreenFadeSubsystem.generated.h"

class AScreenFadeRelay;
class SWidget;
class UUserWidget;
class UWorld;
struct FWorldContext;

/** 가림막이 지금 어느 단계인지. */
UENUM()
enum class EScreenFadePhase : uint8
{
	/** 열려 있다. 가림막은 보이지 않는다. */
	Clear,
	/** 어두워지는 중. */
	FadingOut,
	/** 완전히 어둡다. 트래블 직전이나 로드 중. */
	Covered,
	/** 새 맵이 준비되기를 기다리는 중. 완전히 어둡다. */
	WaitingForReady,
	/** 밝아지는 중. */
	FadingIn
};

/**
 * 가림막의 상태 기계. UObject가 아니라 테스트가 월드 없이 돌릴 수 있다.
 *
 * FadingOut → Covered → (로드) → WaitingForReady → FadingIn → Clear 순으로 돈다.
 * 시간은 실제 경과 시간을 받는다. 로드 중에는 게임 시간이 서지만 실제 시간은 가므로,
 * 페이드는 어느 상황에서도 같은 길이다.
 */
USTRUCT()
struct MINTCHOCO_API FScreenFadeState
{
	GENERATED_BODY()

	EScreenFadePhase Phase = EScreenFadePhase::Clear;

	/** 0 = 보임, 1 = 완전히 어두움. */
	float Opacity = 0.0f;

	/** 현재 단계에 들어온 뒤 지난 시간(초). */
	float Elapsed = 0.0f;

	/** 어두워지기 시작한다. 이미 어둡거나 어두워지는 중이면 그대로 둔다. */
	void StartFadeOut();

	/** 즉시 완전히 어둡게 한다. 로드가 시작될 때. */
	void Cover();

	/** 새 맵이 준비되기를 기다린다. 어둡지 않았다면 먼저 어둡게 한다. */
	void StartWaiting();

	/** 밝아지기 시작한다. */
	void StartFadeIn();

	/**
	 * 한 틱 진행한다. bReady는 이번 틱에 새 맵이 준비됐는지, MaxWait은 준비를 기다리는
	 * 상한이다. 대기 중 상한을 넘기면 준비와 무관하게 밝아진다. 이번 틱에 단계가 바뀌었으면 true.
	 */
	bool Tick(float DeltaTime, float FadeDuration, bool bReady, float MaxWait);

	bool IsDark() const { return Phase == EScreenFadePhase::Covered || Phase == EScreenFadePhase::WaitingForReady; }
	bool IsClear() const { return Phase == EScreenFadePhase::Clear; }
};

/**
 * 맵 전환 가림막. 나갈 때 페이드 아웃, 로드 중 검은 화면, 새 맵이 준비되면 페이드 인.
 *
 * GameInstance 서브시스템인 이유는 맵을 넘어 살아남는 자리가 여기뿐이기 때문이다.
 * 가림막 위젯도 GameInstance를 소유자로 만들어 트래블 뒤 새 뷰포트에 같은 것을 다시 얹는다.
 *
 * 트래블은 모두 TravelWithFade / ServerTravelWithFade / ClientTravelWithFade로 시작한다.
 * 서버 트래블은 릴레이 액터의 멀티캐스트로 모든 클라이언트를 먼저 어둡게 하고, 페이드가
 * 끝난 뒤 TravelHold만큼 더 기다렸다가 떠난다. 들어오는 쪽은 엔진의 PreLoadMap에서
 * 어둡게 고정하고 PostLoadMapWithWorld에서 준비 대기로 넘어간다.
 *
 * "준비"는 로컬 컨트롤러와 PlayerState가 있고, 게임 맵이면 내 폰이 빙의됐고, 등록된
 * 홀드(페인트 아틀라스 베이크 등)가 하나도 없을 때다. MaxReadyWait이 지나면 어쨌든 연다.
 */
UCLASS()
class MINTCHOCO_API UScreenFadeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트의 GameInstance에 붙은 서브시스템. 없으면 nullptr. */
	static UScreenFadeSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 페이드 아웃 뒤 트래블한다. 서버 트래블이면 모든 클라이언트를 먼저 어둡게 한다.
	 * 서버 트래블은 권한이 있는 머신에서만 시작된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Screen Fade", meta = (WorldContext = "WorldContextObject"))
	void TravelWithFade(const FString& URL, bool bServerTravel);

	UFUNCTION(BlueprintCallable, Category = "Screen Fade")
	void ServerTravelWithFade(const FString& URL);

	UFUNCTION(BlueprintCallable, Category = "Screen Fade")
	void ClientTravelWithFade(const FString& URL);

	/** 이 머신만 어두워진다. 트래블은 걸지 않는다. Duration 0 이하면 설정값. */
	UFUNCTION(BlueprintCallable, Category = "Screen Fade")
	void FadeOut(float Duration = 0.0f);

	/**
	 * "아직 로딩 중" 표시를 건다. 같은 소유자·이름으로 여러 번 걸면 그만큼 풀어야 한다.
	 * 소유자가 사라지면(월드가 바뀌면) 홀드도 자동으로 무효가 되므로 새 맵에 새어 나가지 않는다.
	 */
	void AddHold(const UObject* Owner, FName Reason);
	void RemoveHold(const UObject* Owner, FName Reason);
	bool HasHolds() const;

	UFUNCTION(BlueprintPure, Category = "Screen Fade")
	bool IsCovered() const { return !State.IsClear(); }

	EScreenFadePhase GetPhase() const { return State.Phase; }

private:
	bool Tick(float DeltaTime);
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 새 맵을 열어도 되는지. 헤더 주석의 조건. */
	bool IsWorldReady(const UWorld* World) const;

	/** 가림막을 만들고 뷰포트에 얹는다. 이미 얹혀 있으면 아무것도 하지 않는다. */
	void EnsureCoverShown();
	void HideCover();
	void ApplyOpacity();

	/** 어두워진 뒤 실제로 떠난다. */
	void PerformQueuedTravel();

	FScreenFadeState State;

	struct FHold
	{
		TWeakObjectPtr<const UObject> Owner;
		FName Reason;
		int32 Count = 0;
	};
	TArray<FHold> Holds;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CoverWidget;

	/** 위젯 클래스가 없을 때의 단색 가림막. */
	TSharedPtr<SWidget> FallbackCover;
	bool bFallbackShown = false;

	FString QueuedURL;
	bool bTravelQueued = false;
	bool bQueuedServerTravel = false;

	/** 어두워진 뒤 TravelHold를 세는 시계. */
	float TravelHoldElapsed = 0.0f;

	/** 준비 대기가 상한으로 끝났을 때 한 번만 경고하려고. */
	bool bWarnedTimeout = false;

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PreLoadHandle;
	FDelegateHandle PostLoadHandle;
};
