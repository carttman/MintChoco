#include "Paint/PaintSplashProfile.h"

#include "Paint/PaintBrushProfile.h"

float UPaintSplashProfile::ComputeMarkRadius(float Speed) const
{
	return DropletBrush ? DropletBrush->ComputeRadius(DropletSplatVolume, Speed) : 0.0f;
}

FPaintRippleShape UPaintSplashProfile::GetRippleShape() const
{
	FPaintRippleShape Shape;
	Shape.Amplitude = bRipple ? RippleAmplitude : 0.0f;
	Shape.Speed = RippleSpeed;
	Shape.Wavelength = RippleWavelength;
	Shape.Decay = RippleDecay;
	return Shape;
}
