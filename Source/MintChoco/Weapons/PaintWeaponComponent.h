#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"

#include "Weapons/PaintWeaponProfile.h"

#include "PaintWeaponComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponFiredSignature, int32, Seed);

/**
 * The trigger side of a paint weapon: holds one profile and turns "trigger pulled" into the
 * profile's shots at the profile's cadence, from the owner's muzzle towards the owner's view.
 * Everything about what flies and how it paints lives in the profile; this component only
 * decides when to call it and where from, so any pawn that adds it and sets a profile can paint.
 *
 * Shots are fired on the machine that pulls the trigger. Every shot already carries a seed, so
 * server authority later means passing that seed in an RPC and replaying the same spread.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UPaintWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Swapping while the trigger is held releases it first, so the old profile's stroke never leaks into the new one. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void SetProfile(UPaintWeaponProfile* NewProfile);

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	UPaintWeaponProfile* GetProfile() const { return Profile; }

	/** The id every shot paints with: the owner's team, set when the weapon is equipped or the team assigned. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void SetPaintId(uint8 NewPaintId) { PaintId = NewPaintId; }

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	uint8 GetPaintId() const { return PaintId; }

	/** Fires once right away, then keeps firing at the profile's cadence until ReleaseTrigger. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void PullTrigger();

	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void ReleaseTrigger();

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	bool IsTriggerHeld() const { return bTriggerHeld; }

	/** Debug: pin every shot to one seed, or return to a fresh seed per shot. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void SetSeedOverride(bool bInUseFixedSeed, int32 InFixedSeed);

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	bool IsUsingFixedSeed() const { return bUseFixedSeed; }

	/** Seed the next shot will use. */
	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	int32 GetNextSeed() const { return NextSeed; }

	/** Where the next shot leaves from, in world space. Falls back to the view when the owner has no muzzle socket. */
	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	FTransform GetMuzzleTransform() const;

	/** Raised after every shot the profile accepted, with the seed it used. Feedback (animation, sound) hangs here. */
	UPROPERTY(BlueprintAssignable, Category = "Paint|Weapon")
	FPaintWeaponFiredSignature OnFired;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** What this weapon fires. Unset means the trigger does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	TObjectPtr<UPaintWeaponProfile> Profile;

	/** Socket on the owner's skeletal mesh that shots leave from. Missing socket: the view point, pushed forward by MuzzleFallbackOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	FName MuzzleSocketName = TEXT("hand_r");

	/** Keeps a socketless muzzle out of the owner's own collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MuzzleFallbackOffset = 60.0f;

private:
	bool FireOnce();
	void OnShotTimer();
	bool BuildContext(FPaintFireContext& OutContext) const;
	APawn* GetOwnerPawn() const;
	void GetOwnerView(FVector& OutOrigin, FVector& OutDirection) const;

	FPaintStrokeState Stroke;
	FTimerHandle ShotTimer;
	bool bTriggerHeld = false;
	bool bUseFixedSeed = false;
	uint8 PaintId = 0;
	int32 NextSeed = 0;
};
