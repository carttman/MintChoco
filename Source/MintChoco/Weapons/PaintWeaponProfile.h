#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

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
	Continuous
};

/** One shot's worth of input, sampled by the weapon right before it fires its profile. */
struct FPaintFireContext
{
	UWorld* World = nullptr;
	APawn* Instigator = nullptr;
	FTransform Muzzle;
	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	uint8 PaintId = 0;
	int32 Seed = 0;
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
	/** Fires once from the context. Returns true when a splat was produced or a projectile launched. */
	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke) const
		PURE_VIRTUAL(UPaintWeaponProfile::Fire, return false;);

	/** Warns once, at equip time, about asset references that would otherwise fail as "nothing happens". */
	virtual void LogUnsetReferences(const UObject* Owner) const {}

	/** Whether the weapon keeps firing after the first shot while the trigger is held. */
	bool RepeatsWhileHeld() const { return FireMode != EPaintFireMode::Single; }

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
};
