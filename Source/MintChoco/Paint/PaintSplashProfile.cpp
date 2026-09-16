#include "Paint/PaintSplashProfile.h"

#include "Paint/PaintBrushProfile.h"

float UPaintSplashProfile::ComputeMarkRadius(float Speed) const
{
	return DropletBrush ? DropletBrush->ComputeRadius(DropletSplatVolume, Speed) : 0.0f;
}
