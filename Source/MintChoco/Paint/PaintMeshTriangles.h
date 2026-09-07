#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;

/** CPU copy of one material slot's triangles from a static mesh's LOD 0, in mesh local space. */
struct MINTCHOCO_API FPaintMeshTriangles
{
	TArray<FVector3f> Positions;

	/** One per position, or empty when the mesh carries none. They only settle which way a triangle faces. */
	TArray<FVector3f> Normals;

	/** Flat triangle list into Positions. */
	TArray<uint32> Indices;
};

/**
 * Reads the slot's triangles from the render data's CPU buffers, which is what both the coverage
 * grid and the atlas bake consume. In a cooked build these buffers only exist if the asset has
 * Allow CPU Access on. Logs the reason and returns false when there is nothing to read.
 */
MINTCHOCO_API bool GatherPaintMeshTriangles(const UStaticMeshComponent& Mesh, int32 MaterialSlot, FPaintMeshTriangles& Out);
