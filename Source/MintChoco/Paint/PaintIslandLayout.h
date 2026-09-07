#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"

/**
 * One planar island of a paint atlas: the mesh seen along one local axis, packed into a
 * rectangle of the atlas. The raster that fills the atlas, the brush that draws rectangles into
 * it and the surface shader that reads it all go through this mapping, so a surface point lands
 * on the same texel for all three.
 *
 * The plane axes follow the projection axis: X sees (Y, Z), Y sees (X, Z), Z sees (X, Y). The
 * negative direction of an axis uses the same plane, only a different rectangle and the opposite
 * depth sign.
 */
struct MINTCHOCO_API FPaintIsland
{
	EPaintFaceDirection Direction = EPaintFaceDirection::Up;

	/** Projection axis and the two plane axes, as local-space component indices. */
	int32 Axis = 2;
	int32 AxisB = 0;
	int32 AxisC = 1;

	/** +1 keeps the surface with the largest coordinate along Axis, -1 the smallest. */
	int32 Sign = 1;

	/** Island rectangle in atlas texels, gutter included. Max is exclusive. */
	FIntRect Rect;

	/** Atlas texel where the plane's normalized (0, 0) lands. */
	FIntPoint ContentOrigin = FIntPoint::ZeroValue;

	/** Texels the plane's full extent spans on each axis; fractional, since extents rarely divide by the texel. */
	FVector2D ContentTexels = FVector2D::ZeroVector;

	static void PlaneAxes(int32 Axis, int32& OutAxisB, int32& OutAxisC);

	/** Atlas texel coordinate, fractional, of a bounds-normalized local position. */
	FVector2D ProjectNormalized(const FVector& Normalized) const
	{
		return FVector2D(ContentOrigin) + FVector2D(Normalized[AxisB], Normalized[AxisC]) * ContentTexels;
	}

	/** (uv offset, uv scale) the shader applies to the normalized plane coordinates: UV = xy + n.bc * zw. */
	FVector4f ToShaderParam(int32 AtlasSize) const;
};

/**
 * Where every enabled direction's island sits in a surface's atlas, and how big the atlas is.
 * Built once per surface from the mesh bounds and the actor scale; two surfaces with equal
 * layouts can share one baked atlas.
 */
struct MINTCHOCO_API FPaintIslandLayout
{
	/** Enabled directions only, in enum order. */
	TArray<FPaintIsland> Islands;

	/** Square atlas edge in texels, a power of two. Zero when no direction is enabled. */
	int32 AtlasSize = 0;

	/** World size of one texel. Coarser than requested when the islands would not fit MaxSize. */
	float TexelCm = 0.0f;

	uint8 EnabledDirections = 0;

	/**
	 * Lays out one island per enabled direction. An island's content spans the mesh's world
	 * footprint along its plane (local extent times scale) at RequestedTexelCm per texel, padded
	 * by PadTexels on every side; the islands are shelf-packed into the smallest power-of-two
	 * square between MinSize and MaxSize. When even MaxSize cannot hold them the texel grows
	 * until it does.
	 */
	static FPaintIslandLayout Build(
		const FBox& LocalBounds,
		const FVector& Scale3D,
		uint8 InEnabledDirections,
		float RequestedTexelCm,
		int32 PadTexels,
		int32 MinSize,
		int32 MaxSize);

	const FPaintIsland* Find(EPaintFaceDirection Direction) const;
	bool IsEmpty() const { return Islands.IsEmpty(); }

	/** Same mask, atlas size, texel and rectangles hash the same, which is what atlas sharing keys on. */
	uint32 ComputeHash() const;

	FString ToString() const;
};
