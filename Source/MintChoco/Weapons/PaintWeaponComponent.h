#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"

#include "Weapons/PaintWeaponProfile.h"

#include "PaintWeaponComponent.generated.h"

class APawn;
class UInkTankComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponFiredSignature, int32, Seed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponPaintIdSignature, uint8, PaintId);

/**
 * The trigger side of a paint weapon: holds one profile and turns "trigger pulled" into the
 * profile's shots at the profile's cadence, from the owner's muzzle towards the owner's view.
 * Everything about what flies and how it paints lives in the profile; this component only
 * decides when to call it and where from, so any pawn that adds it and sets a profile can paint.
 * When the owner carries an ink tank, every accepted shot also spends the profile's cost from it,
 * and an empty tank refuses the shot before the profile ever sees it.
 *
 * The owning machine decides when a shot happens (trigger, cadence, stroke spacing); the server
 * decides what it does. A client runs the profile without authority, which only reports whether
 * the shot is due, then sends its seed and view to the server, which fires for real. The
 * balls other machines see are cosmetic replays of the shot the server accepted: the owner
 * spawns its own at trigger time, everyone else on the shot multicast.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UPaintWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Swapping while the trigger is held releases it first, so the old profile's stroke never leaks into the new one. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void SetProfile(UPaintWeaponProfile* NewProfile);

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	UPaintWeaponProfile* GetProfile() const { return Profile; }

	/**
	 * The id every shot paints with: the owner's team, set when the weapon is equipped or the team
	 * assigned. Replicated like the profile, so a change made on the owning client reaches the
	 * server that fires and every machine that colours the owner by it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void SetPaintId(uint8 NewPaintId);

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

	/** Raised on every machine whose copy of the paint id changed. The owner's ink bottle recolours from here. */
	UPROPERTY(BlueprintAssignable, Category = "Paint|Weapon")
	FPaintWeaponPaintIdSignature OnPaintIdChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** What this weapon fires. Unset means the trigger does nothing. Replicated so every machine replays the same balls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Profile, Category = "Paint|Weapon")
	TObjectPtr<UPaintWeaponProfile> Profile;

	/** Socket on the owner's skeletal mesh that shots leave from. Missing socket: the view point, pushed forward by MuzzleFallbackOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	FName MuzzleSocketName = TEXT("hand_r");

	/** Keeps a socketless muzzle out of the owner's own collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MuzzleFallbackOffset = 60.0f;

	UPROPERTY(ReplicatedUsing = OnRep_PaintId)
	uint8 PaintId = 0;

	/**
	 * Tags on the owner's ability system that refuse the trigger, on the owner and on the server
	 * alike. An item effect that takes the weapon away (the spinner) is such a tag. The weapon
	 * knows nothing else about abilities; an owner without an ability system is never blocked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	FGameplayTagContainer TriggerBlockedTags;

	UFUNCTION()
	void OnRep_Profile();

	UFUNCTION()
	void OnRep_PaintId();

private:
	bool FireOnce();
	void OnShotTimer();
	bool HasAuthority() const;
	bool IsTriggerBlocked() const;
	float GetShotCost() const;
	bool CanAffordShot() const;
	void SpendShot();
	void BuildContext(FPaintFireContext& OutContext, const FVector& ViewOrigin, const FVector& ViewDirection) const;
	FTransform ComputeMuzzleTransform(const FVector& ViewOrigin, const FVector& ViewDirection) const;
	APawn* GetOwnerPawn() const;
	void GetOwnerView(FVector& OutOrigin, FVector& OutDirection) const;

	/** The owner's shot, fired for real with the server's muzzle and the view the owner aimed with. */
	UFUNCTION(Server, Reliable)
	void ServerFire(int32 Seed, FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);

	UFUNCTION(Server, Reliable)
	void ServerSetProfile(UPaintWeaponProfile* NewProfile);

	UFUNCTION(Server, Reliable)
	void ServerSetPaintId(uint8 NewPaintId);

	/** Replays the cosmetic side of an accepted shot on machines that have no ball of their own yet. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFired(const FPaintShot& Shot);

	/** The owner's ink reserve, found at BeginPlay. Unset means the owner shoots for free. */
	TWeakObjectPtr<UInkTankComponent> Tank;

	FPaintStrokeState Stroke;
	FTimerHandle ShotTimer;
	bool bTriggerHeld = false;
	bool bUseFixedSeed = false;
	int32 NextSeed = 0;
};
