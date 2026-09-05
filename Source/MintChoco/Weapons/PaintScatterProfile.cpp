#include "Weapons/PaintScatterProfile.h"

#include "Math/RandomStream.h"

void UPaintScatterProfile::ComputePelletDirections(const FVector& AimDirection, int32 Seed, TArray<FVector>& OutDirections) const
{
	const FVector Aim = AimDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const float Jitter = FMath::DegreesToRadians(SpreadHalfAngleDeg);
	const int32 Pellets = FMath::Max(PelletsPerShot, 1);
	const FRandomStream Stream(Seed);

	OutDirections.Reset(Pellets);
	for (int32 Pellet = 0; Pellet < Pellets; ++Pellet)
	{
		FVector Center = Aim;
		if (Pattern == EPaintScatterPattern::HorizontalFan && Pellets > 1)
		{
			// Yaw about the world up axis, so the fan stays level however far the aim pitches.
			const float Yaw = FMath::Lerp(-FanHalfAngleDeg, FanHalfAngleDeg, static_cast<float>(Pellet) / (Pellets - 1));
			Center = FRotator(0.0f, Yaw, 0.0f).RotateVector(Aim);
		}
		OutDirections.Add(Jitter > 0.0f ? Stream.VRandCone(Center, Jitter) : Center);
	}
}
