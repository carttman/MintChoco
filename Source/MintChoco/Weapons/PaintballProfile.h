#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "Weapons/PaintDeposit.h"

#include "PaintballProfile.generated.h"

class APaintProjectile;
class APawn;
class UNiagaraSystem;

/**
 * What a paintball is: the actor that flies, its size and weight, and what it leaves where it
 * lands. It says nothing about how many are fired or how they spread; a gun profile pairs one of
 * these with a scatter profile, so the same ball can be lobbed one at a time or blasted as pellets.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintballProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Warns once, at equip time, about references that would otherwise fail as "nothing happens". */
	void LogUnsetReferences(const UObject* Owner) const;

	/**
	 * Spawns one ball at the transform, already moving. A cosmetic ball flies and dies the same
	 * way but never paints: it is a client's picture of a ball the server owns. Returns null when
	 * ProjectileClass is unset or the spawn was refused.
	 */
	APaintProjectile* Launch(UWorld& World, const FTransform& SpawnTransform, APawn* Instigator,
		const FVector& Velocity, uint8 PaintId, int32 Seed, bool bCosmetic) const;

	/** The actor that flies. Its Blueprint sets the mesh; radius and gravity come from here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball")
	TSubclassOf<APaintProjectile> ProjectileClass;

	/** Collision and visual radius of the ball. The splat's size comes from Deposit instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball", meta = (ClampMin = "1", ForceUnits = "cm"))
	float Radius = 6.0f;

	/** 0 flies straight, 1 drops like a thrown object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball", meta = (ClampMin = "0", ForceUnits = "x"))
	float GravityScale = 0.5f;

	/**
	 * Seconds of flight before gravity switches to DropGravityScale. 0 - the default - keeps one
	 * gravity for the whole flight, which is what every ball did before this existed.
	 *
	 * This is how a gun states its reach: the ball flies where it was aimed, then falls out of the
	 * air rather than vanishing at an invisible line. Straight distance is muzzle speed x this,
	 * so the value reads as a range once the gun's speed is known.
	 *
	 * Every machine runs the switch off the same timer from the same launch, so the server's ball
	 * and the clients' cosmetic ones fall together. Leave it 0 on anything whose reach is set by
	 * the ballistic range formula instead (the burst items), or their radius stops matching.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball", meta = (ClampMin = "0", ForceUnits = "s"))
	float DropAfter = 0.0f;

	/** Gravity once DropAfter has passed. Higher reads as a sharper break in the arc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball",
		meta = (ClampMin = "0", ForceUnits = "x", EditCondition = "DropAfter > 0"))
	float DropGravityScale = 4.0f;

	/** What the ball leaves where it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball")
	FPaintDeposit Deposit;

	/** Whether this ball paints as it flies. False when the trail deposit has no brush profile. */
	bool HasTrail() const { return TrailDeposit.CanPaint() && TrailRayCount > 0 && MaxTrailSplats > 0; }

	/**
	 * What the ball leaves on the surfaces it passes on the way, sampled every TrailSpacing along
	 * the flight path. Leave the brush profile unset - the default - and the ball paints only
	 * where it lands, which is what every ball did before this existed.
	 *
	 * A trail costs TrailRayCount traces per sample and up to MaxTrailSplats replicated splats per
	 * ball, and the splat log is replayed in full by a client that joins late. That is why it is
	 * opt-in per ball rather than a flag on the gun: a 15-pellet volley at 10 Hz must not have one.
	 * Keep HitPower at 0 here, or a ball flying past a balloon would strike it once per sample.
	 */
	/**
	 * 이 공이 어딘가에 떨어질 때 그 자리에서 한 번 터지는 이펙트. 비어 있으면 아무것도 하지 않는다.
	 *
	 * RPC가 없다: 머신마다 자기 공이 있고(서버는 진짜, 나머지는 연출용) 각자 제 OnHit에서
	 * 그리므로, 한 번의 착탄이 각 화면에 한 번씩 보인다. 무기별로 켜고 끄라고 공 프로필에 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|FX")
	TObjectPtr<UNiagaraSystem> ImpactFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|FX", meta = (ClampMin = "0.01"))
	float ImpactFXScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail")
	FPaintDeposit TrailDeposit;

	/** Distance flown between two trail samples. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail", meta = (ClampMin = "10", ForceUnits = "cm"))
	float TrailSpacing = 200.0f;

	/** How far from the flight path a sample reaches for a surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail", meta = (ClampMin = "1", ForceUnits = "cm"))
	float TrailRadius = 250.0f;

	/**
	 * Rays per sample, spread evenly around the plane across the flight direction. Four already
	 * cover floor, ceiling and both sides whichever way the ball is heading, and every ray is a
	 * trace, so this is the knob that decides what a trail costs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail", meta = (ClampMin = "1", ClampMax = "16"))
	int32 TrailRayCount = 4;

	/** Hard cap on trail splats one ball may leave, whatever its flight time says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail", meta = (ClampMin = "0"))
	int32 MaxTrailSplats = 8;

	/**
	 * Skips surfaces that would only show a passing effect - a wall, since paint runs off it -
	 * so the trail marks the floors and platforms that keep paint. Without this, a ball flying
	 * along a wall spawns one side-splat actor per sample per ray.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball|Trail")
	bool bTrailSkipTransient = true;
};
