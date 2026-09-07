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

private:
	UFUNCTION()
	void OnRep_Ink();

	void ApplyInk(float NewInk);

	/** The reserve a pawn spawns with, and the live value while playing. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Ink, Category = "Ink", meta = (ClampMin = "0", ClampMax = "1"))
	float Ink = 1.0f;

	/** Seconds of refill still paused by the last spend. */
	float RefillPause = 0.0f;
};
