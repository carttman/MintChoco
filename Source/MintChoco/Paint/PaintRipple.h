#pragma once

#include "CoreMinimal.h"

/**
 * One ring wave through the paint around a contact, as the surface material shades it: the crest
 * runs out at Speed, the envelope dies at Decay, and one Wavelength of rings trails the crest.
 * World cm and seconds; MF_PaintOverlay's PaintRipple node evaluates the same function.
 */
struct MINTCHOCO_API FPaintRippleShape
{
	float Amplitude = 2.5f;
	float Speed = 80.0f;
	float Wavelength = 14.0f;
	float Decay = 2.0f;

	/** Seconds until the wave has fallen below PaintRipple::Cutoff of its amplitude. */
	float Lifetime() const;
};

/** A wave in flight on one surface: where it started in the surface's scaled-local frame, and when. */
struct MINTCHOCO_API FPaintRippleWave
{
	FVector Center = FVector::ZeroVector;
	float StartTime = -1.0e6f;
	FPaintRippleShape Shape;

	bool IsAlive(float Now) const;
};

namespace PaintRipple
{
	/** Waves one surface keeps at once; a new one takes a dead slot, else the oldest. */
	inline constexpr int32 SlotCount = 4;

	/** Fraction of the amplitude below which a wave counts as gone. */
	inline constexpr float Cutoff = 0.02f;

	/** Height of the wave, cm, at Distance from its centre and Age seconds after it started, with the slope along the distance. Zero before birth. */
	MINTCHOCO_API float Height(const FPaintRippleShape& Shape, float Distance, float Age, float& OutSlope);

	/** The slot a new wave takes: the first dead one, else the oldest. */
	MINTCHOCO_API int32 PickSlot(TArrayView<const FPaintRippleWave> Waves, float Now);
}

struct MINTCHOCO_API FPaintRippleSlots
{
	FPaintRippleWave Waves[PaintRipple::SlotCount];

	/** Stores the wave and returns the slot it took. */
	int32 Push(const FPaintRippleWave& Wave, float Now);
};
