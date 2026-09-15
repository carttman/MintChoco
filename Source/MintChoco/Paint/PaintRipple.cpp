#include "Paint/PaintRipple.h"

float FPaintRippleShape::Lifetime() const
{
	return FMath::Loge(1.0f / PaintRipple::Cutoff) / FMath::Max(Decay, UE_KINDA_SMALL_NUMBER);
}

bool FPaintRippleWave::IsAlive(float Now) const
{
	return Shape.Amplitude > 0.0f && Now >= StartTime && Now - StartTime <= Shape.Lifetime();
}

float PaintRipple::Height(const FPaintRippleShape& Shape, float Distance, float Age, float& OutSlope)
{
	OutSlope = 0.0f;
	if (Age < 0.0f || Shape.Amplitude <= 0.0f)
	{
		return 0.0f;
	}
	// A crest running out at Speed under a Gaussian window one wavelength wide, dying at Decay:
	// h = A e^(-Decay t) e^(-u^2 / 2 sigma^2) cos(k u), u = r - Speed t.
	const float Sigma = FMath::Max(Shape.Wavelength, 1e-3f);
	const float Wavenumber = 2.0f * UE_PI / Sigma;
	const float U = Distance - Shape.Speed * Age;
	const float Envelope = Shape.Amplitude * FMath::Exp(-Shape.Decay * Age) * FMath::Exp(-U * U / (2.0f * Sigma * Sigma));
	const float Cos = FMath::Cos(Wavenumber * U);
	const float Sin = FMath::Sin(Wavenumber * U);
	OutSlope = Envelope * (-(U / (Sigma * Sigma)) * Cos - Wavenumber * Sin);
	return Envelope * Cos;
}

int32 PaintRipple::PickSlot(TArrayView<const FPaintRippleWave> Waves, float Now)
{
	int32 Oldest = 0;
	for (int32 Index = 0; Index < Waves.Num(); ++Index)
	{
		if (!Waves[Index].IsAlive(Now))
		{
			return Index;
		}
		if (Waves[Index].StartTime < Waves[Oldest].StartTime)
		{
			Oldest = Index;
		}
	}
	return Oldest;
}

int32 FPaintRippleSlots::Push(const FPaintRippleWave& Wave, float Now)
{
	const int32 Slot = PaintRipple::PickSlot(Waves, Now);
	Waves[Slot] = Wave;
	return Slot;
}
