#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PaintScatterProfile.generated.h"

/**
 * How paintballs leave the muzzle: how many per shot, how fast, and how wide a cone they spread
 * over. It never decides what a ball is; a gun profile pairs one of these with a paintball profile.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintScatterProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Pellet directions for one shot. Deterministic in the seed, so a server can roll a shot once
	 * and every client replays the same spread.
	 */
	void ComputePelletDirections(const FVector& AimDirection, int32 Seed, TArray<FVector>& OutDirections) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float MuzzleSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "1"))
	int32 PelletsPerShot = 1;

	/** Cone half-angle every pellet is scattered within. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float SpreadHalfAngleDeg = 2.0f;
};
