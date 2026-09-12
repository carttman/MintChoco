#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/NetSerialization.h"

#include "PaintWeaponProfile.generated.h"

class APawn;
class UNiagaraSystem;

/** How long one trigger pull lasts. Part of the profile, so one asset says when it fires as well as what flies. */
UENUM(BlueprintType)
enum class EPaintFireMode : uint8
{
	/** One shot per press. */
	Single,
	/** Repeats at ShotsPerSecond while the trigger is held. */
	Automatic,
	/** Fires every tick while held; a brush stroke throttles itself by distance instead of by time. */
	Continuous,
	/** Fires once when the trigger is released after being held for at least MinChargeToFire of ChargeTime; released earlier, nothing happens. */
	Charged
};

/**
 * One shot's worth of input, sampled by the weapon right before it fires its profile. On the
 * server the view comes from the owning client's RPC, the muzzle from the server's own pawn.
 */
struct FPaintFireContext
{
	UWorld* World = nullptr;
	APawn* Instigator = nullptr;
	FTransform Muzzle;
	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	uint8 PaintId = 0;
	int32 Seed = 0;

	/** How charged a Charged shot was on release, 0 to 1. Always 1 for the other fire modes. Scales the stun a hit carries. */
	float ChargeFraction = 1.0f;

	/**
	 * False on a client: the profile only decides whether this shot would fire and describes it,
	 * without launching or painting anything. The weapon turns a true result into a server RPC.
	 */
	bool bAuthority = true;
};

/**
 * What one accepted shot looked like, resolved: enough for another machine to replay its
 * cosmetic side (the balls that fly) without redoing the aim trace. The seed makes the replayed
 * scatter identical to the authoritative one.
 */
USTRUCT()
struct FPaintShot
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Muzzle = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	/** How far along Direction the shot reached, for a hitscan's tracer. A projectile leaves it 0. */
	UPROPERTY()
	float Distance = 0.0f;

	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	uint8 PaintId = 0;

	/**
	 * How charged the shot was, in 1/255 steps; 255 for every non-charged mode. The machines that
	 * only replay the shot have no press of their own to measure, so the muzzle FX scale rides here.
	 */
	UPROPERTY()
	uint8 Charge = 255;
};

/**
 * State a profile keeps between the shots of one trigger hold. Profiles are shared assets and
 * therefore stateless, so the weapon owns this and resets it when the trigger is released.
 */
struct FPaintStrokeState
{
	FVector Anchor = FVector::ZeroVector;
	bool bAnchorValid = false;

	void Reset() { bAnchorValid = false; }
};

/**
 * What a weapon holds: the whole of "how this weapon paints", so plugging a profile into any
 * weapon actor makes it behave the same. The weapon owns the trigger and the muzzle; on each shot
 * it fills a context and calls Fire. Profiles are templates - pick a shipped one, or tune a copy.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API UPaintWeaponProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Fires once from the context. Returns true when the shot was accepted: with authority a splat
	 * was produced or a projectile launched, without it the shot merely would have been. OutShot
	 * describes the accepted shot either way.
	 */
	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const
		PURE_VIRTUAL(UPaintWeaponProfile::Fire, return false;);

	/** Replays the visible side of a shot another machine accepted. Nothing here may paint. */
	virtual void PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const {}

	/** Warns once, at equip time, about asset references that would otherwise fail as "nothing happens". */
	virtual void LogUnsetReferences(const UObject* Owner) const {}

	/** Whether the weapon keeps firing after the first shot while the trigger is held. */
	bool RepeatsWhileHeld() const
	{
		return FireMode == EPaintFireMode::Automatic || FireMode == EPaintFireMode::Continuous;
	}

	/**
	 * Seconds between shots. 0 means Continuous, which fires once per tick.
	 *
	 * Single reads it too, as a floor between two pulls: without one a click-spammer fires as fast
	 * as the mouse reports, which is what the shotgun did before this existed. Charged paces itself
	 * with ChargeTime instead and has no interval.
	 */
	float GetShotInterval() const
	{
		switch (FireMode)
		{
		case EPaintFireMode::Automatic:
		case EPaintFireMode::Single:
			return 1.0f / FMath::Max(ShotsPerSecond, 0.1f);
		default:
			return 0.0f;
		}
	}

	/**
	 * Fraction of a full ink tank one accepted shot spends; a brush pays it per stamp. 0 fires for
	 * free, and so does a pawn that carries no tank at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Ink")
	float GetInkCostPerShot() const { return InkCostPercent * 0.01f; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence")
	EPaintFireMode FireMode = EPaintFireMode::Single;

	/** Shots per second in Automatic. Continuous ignores it: a brush spaces its stamps by distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence",
		meta = (ClampMin = "0.1", ClampMax = "60.0", ForceUnits = "Hz", EditCondition = "FireMode == EPaintFireMode::Automatic"))
	float ShotsPerSecond = 8.0f;

	/** How long the trigger must be held before releasing it fires, in Charged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence",
		meta = (ClampMin = "0.05", ForceUnits = "s", EditCondition = "FireMode == EPaintFireMode::Charged"))
	float ChargeTime = 3.0f;

	/**
	 * Charge fraction (held time / ChargeTime) the trigger needs on release to fire at all, in Charged.
	 * 1 fires only a full charge; lower lets a partial charge fire, with the profile scaling its
	 * effect (a sniper's stun) by the fraction it got.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence",
		meta = (ClampMin = "0", ClampMax = "1", EditCondition = "FireMode == EPaintFireMode::Charged"))
	float MinChargeToFire = 1.0f;

	/**
	 * Played once per accepted shot, on every machine that renders the shooter. Attached to the
	 * owner's muzzle socket so it follows the gun; unset plays nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	/** Uniform scale MuzzleFX spawns at, before any charge scaling. 1 is the asset's own size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (ClampMin = "0.01"))
	float MuzzleFXScale = 1.0f;

	/**
	 * In Charged, the multiplier at MinChargeToFire (X) and at a full charge (Y). A weapon that
	 * only fires at a full charge can reach Y alone, so lowering MinChargeToFire is what makes the
	 * range visible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (EditCondition = "FireMode == EPaintFireMode::Charged"))
	FVector2D MuzzleFXChargeScale = FVector2D(0.5, 1.5);

	/**
	 * Looping FX for a Charged weapon's hold: spawned at the muzzle when the trigger goes down and
	 * switched off when the shot leaves or the hold is cancelled. Unset shows nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (EditCondition = "FireMode == EPaintFireMode::Charged"))
	TObjectPtr<UNiagaraSystem> ChargeFX;

	/** Uniform scale ChargeFX spawns at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX",
		meta = (ClampMin = "0.01", EditCondition = "FireMode == EPaintFireMode::Charged"))
	float ChargeFXScale = 1.0f;

	/**
	 * Uniform scale one shot's muzzle FX spawns at. Charged walks between the ends of
	 * MuzzleFXChargeScale by how far past MinChargeToFire the shot got; every other mode is fixed.
	 */
	UFUNCTION(BlueprintPure, Category = "FX")
	float GetMuzzleFXScale(float ChargeFraction) const
	{
		if (FireMode != EPaintFireMode::Charged)
		{
			return MuzzleFXScale;
		}
		const float Minimum = FMath::Clamp(MinChargeToFire, 0.0f, 1.0f);
		// A full-charge-only weapon has one reachable size; dividing by zero here would be it too.
		const float Alpha = Minimum >= 1.0f
			? 1.0f
			: FMath::Clamp((ChargeFraction - Minimum) / (1.0f - Minimum), 0.0f, 1.0f);
		return MuzzleFXScale * FMath::Lerp(static_cast<float>(MuzzleFXChargeScale.X), static_cast<float>(MuzzleFXChargeScale.Y), Alpha);
	}

private:
	/** Percent of a full ink tank one accepted shot spends. Read it through GetInkCostPerShot. */
	UPROPERTY(EditAnywhere, Category = "Ink", meta = (ClampMin = "0", ClampMax = "100", ForceUnits = "%"))
	float InkCostPercent = 4.0f;
};
