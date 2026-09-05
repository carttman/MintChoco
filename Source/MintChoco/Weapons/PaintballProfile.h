#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "Weapons/PaintDeposit.h"

#include "PaintballProfile.generated.h"

class APaintProjectile;
class APawn;

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
	 * Spawns one ball at the transform, already moving. Returns null when ProjectileClass is unset
	 * or the spawn was refused.
	 */
	APaintProjectile* Launch(UWorld& World, const FTransform& SpawnTransform, APawn* Instigator,
		const FVector& Velocity, uint8 PaintId, int32 Seed) const;

	/** The actor that flies. Its Blueprint sets the mesh; radius and gravity come from here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball")
	TSubclassOf<APaintProjectile> ProjectileClass;

	/** Collision and visual radius of the ball. The splat's size comes from Deposit instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball", meta = (ClampMin = "1", ForceUnits = "cm"))
	float Radius = 6.0f;

	/** 0 flies straight, 1 drops like a thrown object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball", meta = (ClampMin = "0"))
	float GravityScale = 0.5f;

	/** What the ball leaves where it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paintball")
	FPaintDeposit Deposit;
};
