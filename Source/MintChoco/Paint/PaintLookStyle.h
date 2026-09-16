#pragma once

#include "CoreMinimal.h"

/**
 * The look scalars a paint surface hands its material at runtime. Every default is "the material
 * as authored", so a surface that never hears from the style command renders exactly as it
 * compiled. All of it is cosmetic: none of these reaches the paint buffer or the score grid.
 */
struct FPaintLookStyle
{
	/** Multiplies the wet coat's weight. 0 takes the clear coat off entirely. */
	float CoatScale = 1.0f;

	/** Multiplies the paint slab's fuzz. Above 1 the surface reads as snow or sugar rather than ink. */
	float FuzzScale = 1.0f;

	/** Added to the paint slab's roughness, saturated in the shader. Matters most once the coat is off. */
	float RoughnessBias = 0.0f;

	/** Widens the height read so the paint reads as a liquid that flowed. 0 keeps the texel-sharp height. */
	float Flow = 0.0f;

	/** Scales the whole derived normal. 0 leaves the vertex normal, flattening the relief along with the noise. */
	float NormalStrength = 1.0f;
};
