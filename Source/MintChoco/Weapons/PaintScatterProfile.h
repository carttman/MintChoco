#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PaintScatterProfile.generated.h"

/** Where the pellets of one shot are placed before the random jitter is added on top. */
UENUM(BlueprintType)
enum class EPaintScatterPattern : uint8
{
	/** Every pellet starts on the aim direction; SpreadHalfAngleDeg alone spreads them into a cone. */
	Cone,
	/** Pellets are spaced evenly across a horizontal arc of FanHalfAngleDeg to each side of the aim: a wide stripe of paint. */
	HorizontalFan
};

/**
 * How paintballs leave the muzzle: how many per shot, how fast, in what pattern, and how much
 * randomness rides on top. It never decides what a ball is; a gun profile pairs one of these with
 * a paintball profile.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	EPaintScatterPattern Pattern = EPaintScatterPattern::Cone;

	/** Fan reach to each side of the aim, in yaw. The pellets sit at even steps from -Fan to +Fan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter",
		meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg", EditCondition = "Pattern == EPaintScatterPattern::HorizontalFan"))
	float FanHalfAngleDeg = 20.0f;

	/** Random cone each pellet is jittered within, around its pattern position. 0 makes every shot identical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float SpreadHalfAngleDeg = 2.0f;
};
