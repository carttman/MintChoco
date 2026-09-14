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
class UNiagaraComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponFiredSignature, int32, Seed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponPaintIdSignature, uint8, PaintId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPaintWeaponChargingSignature, bool, bCharging);

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

	/** Swapping while the trigger is held cancels it first, so the old profile's stroke or charge never leaks into the new one. */
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

	/**
	 * Fires once right away, then keeps firing at the profile's cadence until ReleaseTrigger. A
	 * Charged profile fires nothing here: it only starts counting the hold.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void PullTrigger();

	/** Lets go of the trigger. A Charged profile held long enough fires its one shot right here. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void ReleaseTrigger();

	/** Lets go without firing, whatever the charge: the hold was interrupted rather than completed. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Weapon")
	void CancelTrigger();

	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	bool IsTriggerHeld() const { return bTriggerHeld; }

	/** 0 to 1 while a Charged profile's trigger is held, reaching 1 once releasing would fire. 0 otherwise. */
	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	float GetChargeFraction() const;

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

	/**
	 * Where the next shot would land, from the owner's current view and muzzle, for the crosshair.
	 * The view it was computed with comes back too, so the caller can tell whether the impact
	 * falls short of the aim point. False when there is no profile or the profile does not predict.
	 * Owner-side only: nothing is launched or spent.
	 */
	bool PredictNextImpact(FVector& OutViewOrigin, FVector& OutViewDirection, FVector& OutAimPoint, FVector& OutImpact) const;

	/**
	 * Where to look for the muzzle socket when the owner carries a weapon mesh of its own. Barrel
	 * lengths differ per character, so the socket belongs on that mesh rather than on a skeleton
	 * several characters share. A missing component or socket falls back to MuzzleSocketName on
	 * the owner's skeletal mesh, so a pawn without a weapon mesh still fires from its hand.
	 */
	void SetMuzzleSource(USceneComponent* Component, FName SocketName);

	/**
	 * Raised once per accepted shot on every machine: on the owner when it predicts the shot, on the
	 * server when it fires for real, on everyone else when the shot multicast lands. Feedback
	 * (animation, sound) hangs here.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Paint|Weapon")
	FPaintWeaponFiredSignature OnFired;

	/**
	 * 이 무기가 캐릭터에게 조준 자세를 요구하는 중인지.
	 *
	 * 방아쇠를 당긴 순간부터 놓을 때까지, 그리고 충전하는 내내 참이다. 애님 인스턴스가
	 * 매 프레임 이것을 읽어 상체를 올린다(bIsFiring 을 읽는 것과 같은 방식).
	 *
	 * 발사 **뒤**의 여운은 여기서 다루지 않는다 — 그쪽은 애님 인스턴스의 bRecentlyFired
	 * (FireHoldTime) 가 이미 맡고 있다. 이 값은 쏘기 **전**과 충전 **중**만 채운다.
	 */
	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	bool IsAiming() const { return bAiming; }

	/** Charged 방아쇠를 누르고 있는 중인지. 복제 값이라 서버와 관전 머신도 같은 답을 본다. */
	UFUNCTION(BlueprintPure, Category = "Paint|Weapon")
	bool IsCharging() const { return bCharging; }

	/** Raised on every machine whose copy of the paint id changed. The owner's ink bottle recolours from here. */
	UPROPERTY(BlueprintAssignable, Category = "Paint|Weapon")
	FPaintWeaponPaintIdSignature OnPaintIdChanged;

	/**
	 * 충전이 시작되고 끝날 때 모든 머신에서. 소유자는 누른 순간, 나머지는 복제가 도착할 때.
	 *
	 * 캐릭터가 이것을 받아 총을 들고 발사 자세를 잡는다. 그러지 않으면 충전 내내 IDLE 포즈라
	 * 총 소켓이 쉬는 손 위치에 있고, 발사 지점을 그 순간의 총구에서 재므로 탄이 거기서 나간다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Paint|Weapon")
	FPaintWeaponChargingSignature OnChargingChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** What this weapon fires. Unset means the trigger does nothing. Replicated so every machine replays the same balls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Profile, Category = "Paint|Weapon")
	TObjectPtr<UPaintWeaponProfile> Profile;

	/** Socket on the owner's skeletal mesh that shots leave from. Missing socket: the view point, pushed forward by MuzzleFallbackOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	FName MuzzleSocketName = TEXT("hand_r");

	/**
	 * 방아쇠를 당기고 첫 발이 나가기까지 기다리는 시간(초). 0 이면 다음 틱에 나간다.
	 *
	 * 총구는 캐릭터 메시의 Gun 소켓을 타므로 **쏘는 순간의 자세**가 발사 지점을 정한다.
	 * 자세를 올리기도 전에 쏘면 첫 발만 내린 손에서 나가고, 연사 중인 다음 발들은 올라간
	 * 총구에서 나가 서로 어긋난다. 그래서 첫 발은 자세가 올라온 뒤로 미룬다.
	 *
	 * 자세가 이미 올라와 있으면(연사 중) 기다리지 않는다. 애님 그래프의 상체 블렌드 시간과
	 * 맞추면 첫 발과 나머지 발의 발사 지점이 같아진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0", ForceUnits = "s"))
	float AimReadyDelay = 0.0f;

	/**
	 * 마지막 발사 뒤 자세가 아직 올라와 있다고 보는 시간(초).
	 *
	 * **UUnitAnimInstance::FireHoldTime 과 같은 값으로 둘 것.** 그쪽이 실제로 자세를 유지하는
	 * 시간이고, 이 값은 “지금 쏘면 자세가 이미 올라와 있는가” 를 판단하는 데만 쓴다. 둘이 어긋나면
	 * 연사 도중에 불필요한 준비 시간이 끼거나, 자세가 내려간 뒤에 기다리지 않고 쏜다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0", ForceUnits = "s"))
	float AimHoldSeconds = 0.5f;

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

	/** Tags on the owner's ability system that make every shot free (the infinite ammo item). The tank is neither checked nor spent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon")
	FGameplayTagContainer FreeShotTags;

	/**
	 * While a free-shot tag is up, the cadence the weapon runs at instead of the profile's.
	 * The infinite ammo item is meant to feel faster, not only cheaper, so the same tag that
	 * waives the ink cost also shortens these two.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float FreeShotInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint|Weapon", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float FreeShotChargeTime = 0.5f;

	UFUNCTION()
	void OnRep_Profile();

	UFUNCTION()
	void OnRep_PaintId();

	UFUNCTION()
	void OnRep_Charging();

private:
	bool FireOnce();
	void OnShotTimer();
	bool HasAuthority() const;
	bool IsTriggerBlocked() const;
	bool IsShotFree() const;

	/** The profile's cadence, shortened while a free-shot tag is up. */
	float GetEffectiveShotInterval() const;

	/** The profile's charge time, shortened while a free-shot tag is up. */
	float GetEffectiveChargeTime() const;
	float GetShotCost() const;
	bool CanAffordShot() const;
	void SpendShot();
	void BuildContext(FPaintFireContext& OutContext, const FVector& ViewOrigin, const FVector& ViewDirection, float ChargeFraction) const;
	FTransform ComputeMuzzleTransform(const FVector& ViewOrigin, const FVector& ViewDirection) const;

	/** The mesh that carries the muzzle socket, or null when the owner has no such socket. */
	USkeletalMeshComponent* GetMuzzleMesh() const;

	/**
	 * 이펙트를 붙일 곳과 그 소켓. 총 메시의 총구 소켓이 있으면 그쪽, 없으면 캐릭터 메시의
	 * 손 소켓. 없으면 null.
	 *
	 * ComputeMuzzleTransform 과 같은 순서로 고른다 — 탄이 총열에서 나가는데 불꽃만 손에서
	 * 피면 둘이 어긋난다.
	 */
	USceneComponent* GetMuzzleAttachment(FName& OutSocket) const;

	/** Plays the profile's muzzle FX once on this machine. Charge only matters in Charged. */
	void PlayMuzzleFX(float ChargeFraction);

	/** Records the hold locally, relays it to the machines that only watch, and drives the FX here. */
	void SetCharging(bool bNewCharging);
	void StartChargeFX();
	void StopChargeFX();

	/** Writes this weapon's team colour into a spawned FX's User.TintColor. Null is ignored. */
	void TintTeamFX(UNiagaraComponent* FX) const;

	/** 충전 상태가 바뀌었다. 이펙트를 켜고 끄고, 캐릭터가 자세를 잡도록 알린다. */
	void ApplyChargingVisuals(bool bNewCharging);

	void SetAiming(bool bNewAiming);

	/** 자세가 이미 올라와 있으면 바로, 아니면 자세를 켜고 준비 시간 뒤에 쏜다. */
	void FireWhenAimReady();

	/** 준비 시간이 끝났다. 미뤄 둔 첫 발을 쏘고, 그 사이 들어온 방아쇠 해제를 뒤늦게 처리한다. */
	void FireAfterAimReady();

	/** 조준 자세를 요구하는 중. 애님 인스턴스가 매 프레임 읽는다. */
	bool bAiming = false;

	/** 준비 시간을 기다리는 첫 발이 있다. 그 사이 방아쇠를 놓아도 이 한 발은 나간다. */
	bool bShotPending = false;

	/** 준비 중에 방아쇠가 풀렸다. 미뤄 둔 발이 나간 직후에 해제를 이어서 처리한다. */
	bool bCancelAfterPendingShot = false;

	FTimerHandle AimReadyTimer;
	APawn* GetOwnerPawn() const;
	void GetOwnerView(FVector& OutOrigin, FVector& OutDirection) const;

	/**
	 * The owner's shot, fired for real with the server's muzzle and the view the owner aimed with.
	 * Charge is the owner's charge fraction in 1/255 steps (255 for every non-charged mode); the
	 * server never saw the press, so it takes the owner's word for it.
	 */
	UFUNCTION(Server, Reliable)
	void ServerFire(int32 Seed, FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, uint8 Charge);

	/**
	 * The owner's hold, relayed so the other machines can show it. The owner never receives its
	 * own copy: it drove the FX from its own press, and a late echo would restart the loop.
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetCharging(bool bNewCharging);

	UFUNCTION(Server, Reliable)
	void ServerSetProfile(UPaintWeaponProfile* NewProfile);

	UFUNCTION(Server, Reliable)
	void ServerSetPaintId(uint8 NewPaintId);

	/** Replays the cosmetic side of an accepted shot on machines that have no ball of their own yet. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFired(const FPaintShot& Shot);

	/** The owner's ink reserve, found at BeginPlay. Unset means the owner shoots for free. */
	TWeakObjectPtr<UInkTankComponent> Tank;

	/** Weapon mesh the muzzle socket sits on. Unset means the owner has no separate weapon mesh. */
	TWeakObjectPtr<USceneComponent> MuzzleSource;

	FName MuzzleSourceSocket = NAME_None;

	FPaintStrokeState Stroke;
	FTimerHandle ShotTimer;

	/** World time the trigger was pulled; a Charged profile measures its hold from here. */
	double PressTime = 0.0;

	/**
	 * World time the last shot left. Single paces itself against this: the trigger fires on the
	 * pull rather than on a timer, so the cadence has to be a floor between two pulls.
	 * Starts far in the past so the first shot of a life is never held back.
	 */
	double LastShotTime = -UE_BIG_NUMBER;

	/** True while a Charged trigger is held. Replicated for the machines that only watch. */
	UPROPERTY(ReplicatedUsing = OnRep_Charging)
	bool bCharging = false;

	/** The hold FX loops, so it has to be switched off by hand rather than expiring. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ChargeFXComponent;

	/** Charge fraction of the shot FireOnce is about to fire. ReleaseTrigger samples it before the cancel clears the hold. */
	float PendingChargeFraction = 1.0f;
	bool bTriggerHeld = false;
	bool bUseFixedSeed = false;
	int32 NextSeed = 0;
};
