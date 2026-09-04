#include "Paint/PaintBrushProfile.h"

#include "Engine/HitResult.h"

FPaintSplat UPaintBrushProfile::BuildSplat(
	const FHitResult& Hit,
	FVector IncidentVelocity,
	uint8 PaintId,
	float Volume,
	float HeightAdd,
	int32 Seed) const
{
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float Speed = IncidentVelocity.Size();
	const FVector Incident = Speed > UE_KINDA_SMALL_NUMBER
		? IncidentVelocity / Speed
		: -Normal;

	const float CosTheta = FMath::Abs(FVector::DotProduct(Incident, Normal));

	const float Radius = FMath::Min(
		BaseRadius * FMath::Sqrt(FMath::Max(Volume, 0.0f)) + RadiusPerSpeed * Speed,
		MaxRadius);
	const float Stretch = FMath::Clamp(1.0f / FMath::Max(CosTheta, UE_KINDA_SMALL_NUMBER), 1.0f, MaxStretch);
	const FVector Tangent = (Incident - FVector::DotProduct(Incident, Normal) * Normal).GetSafeNormal();
	// A grazing hit lands "ahead" of the contact along the tangent.
	const float CenterShift = Radius * (Stretch - 1.0f) * CenterShiftScale;

	FPaintSplat Splat;
	// The shader hashes the seed with sin(), which loses precision past 16 bits; the same
	// truncated value drives the rotation below so the whole shape follows one number.
	Splat.Seed = static_cast<uint16>(Seed & 0xFFFF);

	// The stamp needs a full 2D frame on the surface, not just a stretch axis. A near-round
	// stamp gains nothing from tangent alignment - it would just repeat one orientation every
	// click - so its rotation comes from the seed instead: every client derives the same frame
	// and stamps the same shape.
	FVector AxisU = Tangent;
	if (Stretch < MinAlignedStretch || AxisU.IsNearlyZero())
	{
		const FRandomStream Stream(Splat.Seed);
		const FVector Reference = FMath::Abs(Normal.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector Base = FVector::CrossProduct(Normal, Reference).GetSafeNormal();
		AxisU = Base.RotateAngleAxis(Stream.FRand() * 360.0f, Normal);
	}

	Splat.Location = Hit.ImpactPoint + Tangent * CenterShift;
	Splat.Normal = Normal;
	Splat.AxisU = AxisU;
	Splat.Radius = Radius;
	Splat.Stretch = Stretch;
	// The center slid ahead of the contact, so in stamp space the impact sits behind the
	// origin: its u coordinate, normalized by the stamp's long axis, anchors the spike field.
	Splat.ImpactU = -CenterShift / FMath::Max(Radius * Stretch, UE_KINDA_SMALL_NUMBER);
	Splat.PaintId = PaintId;
	Splat.HeightAdd = HeightAdd;
	Splat.BrushMaterial = BrushMaterial;
	return Splat;
}
