#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintIslandLayout.h"

class UStaticMeshComponent;
class UTexture2D;

/** Everything the bake reads: one material slot's LOD 0 triangles and the layout to fill. */
struct MINTCHOCO_API FPaintAtlasBakeInput
{
	TArray<FVector3f> Positions;

	/** Vertex normals, or empty. They only settle which way a triangle faces. */
	TArray<FVector3f> Normals;

	/** Flat triangle list into Positions. */
	TArray<uint32> Indices;

	FBox LocalBounds = FBox(ForceInit);
	FPaintIslandLayout Layout;
	float EdgeFadeTexels = 8.0f;
	float EdgeFadeSeamFraction = 0.05f;
};

struct MINTCHOCO_API FPaintAtlasBakeOutput
{
	/** AtlasSize^2 texels. xyz is the bounds-normalized local position, w is 1 where a surface was found. */
	TArray<FFloat16Color> Positions;

	/** AtlasSize^2 bytes. 255 well inside a surface, 0 at every edge and wherever there is no surface. */
	TArray<uint8> EdgeFade;

	int32 CoveredTexels = 0;
};

/**
 * Bakes a surface's paint atlas on the CPU: for every island, the mesh triangles are projected
 * along the island's axis and rasterized into the atlas, the outermost surface winning, so each
 * texel learns which point of the mesh it stands for. The edge fade then marks how far each texel
 * is from a surface edge, where displacement has to die out.
 *
 * Nothing here touches the GPU until the two Create*Texture calls, so the bake can run on a
 * worker thread and be tested without a renderer; the results are deterministic on every machine.
 */
namespace PaintAtlasBaker
{
	/**
	 * What a texel without surface stores as its normalized position. The brush un-normalizes it
	 * to a point kilometres outside the mesh, so no stamp ever reaches a gutter texel.
	 */
	inline constexpr float EmptyPosition = -64.0f;

	/**
	 * Copies the CPU vertex and index buffers of one material slot of the mesh's LOD 0. In a
	 * cooked build these only exist if the asset has Allow CPU Access on. Returns false and logs
	 * when there is nothing to bake from.
	 */
	MINTCHOCO_API bool GatherMesh(const UStaticMeshComponent& Mesh, int32 MaterialSlot, FPaintAtlasBakeInput& Out);

	/** Fills AtlasSize^2 texels; w is the coverage. Texels no triangle reaches keep EmptyPosition and w 0. */
	MINTCHOCO_API void Rasterize(const FPaintAtlasBakeInput& In, TArray<FVector4f>& OutPositions);

	/**
	 * Distance to the nearest edge as a 0..255 ramp over FadeTexels. An edge is an uncovered
	 * texel, the atlas border, or a neighbour whose position jumps by more than SeamFraction of
	 * the bounds - a step inside one island.
	 */
	MINTCHOCO_API void ComputeEdgeFade(
		TArrayView<const FVector4f> Positions, int32 AtlasSize, float FadeTexels, float SeamFraction, TArray<uint8>& OutFade);

	/** Rasterize, fade and pack to half precision. Safe on any thread. */
	MINTCHOCO_API void Bake(const FPaintAtlasBakeInput& In, FPaintAtlasBakeOutput& Out);

	/** Game thread only. Point-sampled RGBA16F holding the normalized positions. */
	MINTCHOCO_API UTexture2D* CreatePositionTexture(const FPaintAtlasBakeOutput& Out, int32 AtlasSize, FName Name);

	/** Game thread only. Bilinear single-channel byte texture holding the fade. */
	MINTCHOCO_API UTexture2D* CreateEdgeFadeTexture(const FPaintAtlasBakeOutput& Out, int32 AtlasSize, FName Name);
}
