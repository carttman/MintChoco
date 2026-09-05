#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"

class UWorld;

/**
 * Overlays for looking at the coverage grid in PIE. Nothing here is gameplay: the team looks
 * live in the materials and the scores in the grid; these only draw what the grid already knows.
 */
namespace PaintDebug
{
	/** Debug color for a paint id, PaintIdNone included. */
	MINTCHOCO_API FColor IdColor(uint8 PaintId);

	MINTCHOCO_API const TCHAR* FaceName(EPaintFaceDirection Direction);

	/** One line at the mesh center with the whole surface's coverage, plus one line outside each face. */
	MINTCHOCO_API void DrawCoverageText(
		const UWorld* World,
		const FTransform& MeshTransform,
		float UniformScale,
		const FBox& LocalBounds,
		const FPaintCellGrid& Grid,
		const FString& Label);

	/** Every surface cell as a slab lying on the surface, colored by its paint id. */
	MINTCHOCO_API void DrawCells(
		const UWorld* World,
		const FTransform& MeshTransform,
		float UniformScale,
		const FPaintCellGrid& Grid);
}
