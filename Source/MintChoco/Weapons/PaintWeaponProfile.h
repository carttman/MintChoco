#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/NetSerialization.h"

#include "PaintWeaponProfile.generated.h"

class APawn;

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
	/** Fires once when the trigger is released after being held for ChargeTime; released earlier, nothing happens. */
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

	/** Seconds between shots while held. 0 means Continuous, which fires once per tick. */
	float GetShotInterval() const
	{
		return FireMode == EPaintFireMode::Automatic ? 1.0f / FMath::Max(ShotsPerSecond, 0.1f) : 0.0f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence")
	EPaintFireMode FireMode = EPaintFireMode::Single;

	/** Shots per second in Automatic. Continuous ignores it: a brush spaces its stamps by distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence",
		meta = (ClampMin = "0.1", ClampMax = "60.0", EditCondition = "FireMode == EPaintFireMode::Automatic"))
	float ShotsPerSecond = 8.0f;

	/** How long the trigger must be held before releasing it fires, in Charged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cadence",
		meta = (ClampMin = "0.05", ForceUnits = "s", EditCondition = "FireMode == EPaintFireMode::Charged"))
	float ChargeTime = 3.0f;

	/**
	 * Fraction of a full ink tank one accepted shot spends; a brush pays it per stamp. 0 fires for
	 * free, and so does a pawn that carries no tank at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink", meta = (ClampMin = "0", ClampMax = "1"))
	float InkCostPerShot = 0.04f;
};
