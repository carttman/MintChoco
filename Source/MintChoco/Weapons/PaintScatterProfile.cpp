#include "Weapons/PaintScatterProfile.h"

#include "Math/RandomStream.h"

void UPaintScatterProfile::ComputePelletDirections(const FVector& AimDirection, int32 Seed, TArray<FVector>& OutDirections) const
{
	const FVector Aim = AimDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const float HalfAngle = FMath::DegreesToRadians(SpreadHalfAngleDeg);
	const int32 Pellets = FMath::Max(PelletsPerShot, 1);
	const FRandomStream Stream(Seed);

	OutDirections.Reset(Pellets);
	for (int32 Pellet = 0; Pellet < Pellets; ++Pellet)
	{
		OutDirections.Add(HalfAngle > 0.0f ? Stream.VRandCone(Aim, HalfAngle) : Aim);
	}
}
