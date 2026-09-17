#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "InkTankComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInkTankChangedSignature, float, Ink);

/**
 * The ink a pawn shoots from, as a fraction of a full tank.
 *
 * The server owns the value and replicates it to everyone. The owner also spends locally so its
 * own shots feel immediate, and the next replicated value overwrites whatever that prediction
 * drifted to. Refilling is the server's alone; a remote bottle smooths the replicated steps.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UInkTankComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInkTankComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Current reserve, 0 (empty) to 1 (full). */
	UFUNCTION(BlueprintPure, Category = "Ink")
	float GetInk() const { return Ink; }

	UFUNCTION(BlueprintPure, Category = "Ink")
	bool CanAfford(float Cost) const { return Ink + UE_KINDA_SMALL_NUMBER >= Cost; }

	/**
	 * Spends Cost and pauses the refill. Returns false, spending nothing, when the tank holds less.
	 * With authority this is the truth; on the owner it is a prediction the server's value corrects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink")
	bool TryConsume(float Cost);

	/** Sets the reserve outright, clamped to the tank. Meant for the server: debug, refill stations. */
	UFUNCTION(BlueprintCallable, Category = "Ink")
	void SetInk(float NewInk);

	/** Regains ink for Seconds of not shooting. The tick calls this; tests call it directly. */
	void Refill(float Seconds);

	/** Raised whenever the reserve changes, on every machine. UI and the bottle hang here. */
	UPROPERTY(BlueprintAssignable, Category = "Ink")
	FInkTankChangedSignature OnInkChanged;

	/** Tank fraction regained per second of not shooting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink", meta = (ClampMin = "0"))
	float RefillPerSecond = 0.15f;

	/** Seconds after a spend before the refill resumes, so a held trigger drains instead of hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink", meta = (ClampMin = "0"))
	float RefillDelayAfterSpend = 0.5f;

	/**
	 * 보드(대시)를 타는 동안 회복이 몇 배로 빨라지는가.
	 *
	 * 보드 위에서는 쏘지 못한다(AUnit::HandleDashStateChanged가 방아쇠를 놓는다). 그래서 이 시간은
	 * 언제나 순수한 회복 시간이고, 잉크가 마르면 한 바퀴 달려오는 것이 곧 재장전이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink", meta = (ClampMin = "1", ForceUnits = "x"))
	float DashRefillMultiplier = 3.0f;

	/**
	 * 보드를 타기 시작했다/내렸다고 알린다. 대시 플래그가 실제로 바뀐 머신(소유 클라이언트와
	 * 서버)에서 AUnit이 부른다 — 회복을 돌리는 것은 서버뿐이지만, 소유자도 같은 값을 들고 있어야
	 * 예측이 어긋나지 않는다.
	 *
	 * 잉크통이 유닛을 되짚어 보지 않는 이유는 그래야 이 컴포넌트를 아무 폰에나 붙일 수 있기
	 * 때문이다. 대시를 아는 쪽이 밀어 넣는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ink")
	void SetDashing(bool bNewDashing);

	UFUNCTION(BlueprintPure, Category = "Ink")
	bool IsDashing() const { return bDashing; }

	/** 지금의 회복 속도(초당 탱크 비율). 보드 중에는 DashRefillMultiplier가 곱해진다. */
	UFUNCTION(BlueprintPure, Category = "Ink")
	float GetRefillPerSecond() const;

private:
	UFUNCTION()
	void OnRep_Ink();

	void ApplyInk(float NewInk);

	/** The reserve a pawn spawns with, and the live value while playing. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Ink, Category = "Ink", meta = (ClampMin = "0", ClampMax = "1"))
	float Ink = 1.0f;

	/** Seconds of refill still paused by the last spend. */
	float RefillPause = 0.0f;

	/** 보드를 타는 중. 복제하지 않는다: 이 값을 쓰는 회복은 서버에서만 돌고, 소유자는 제 대시를 안다. */
	bool bDashing = false;
};
