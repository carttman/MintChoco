#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintSplat.h"

#include "PaintCellGrid.generated.h"

class UStaticMeshComponent;

/** The six ways a piece of surface can face in the painted mesh's local frame. */
UENUM(BlueprintType)
enum class EPaintFaceDirection : uint8
{
	Front UMETA(ToolTip = "+X"),
	Back UMETA(ToolTip = "-X"),
	Right UMETA(ToolTip = "+Y"),
	Left UMETA(ToolTip = "-Y"),
	Up UMETA(ToolTip = "+Z"),
	Down UMETA(ToolTip = "-Z"),
};

inline constexpr int32 PaintFaceDirectionCount = 6;

/** Unit vector, in mesh local space, that the direction faces. */
MINTCHOCO_API FVector PaintFaceDirectionVector(EPaintFaceDirection Direction);

/** The direction whose axis the normal leans on most. */
MINTCHOCO_API EPaintFaceDirection ClassifyPaintFaceDirection(const FVector& Normal);

/** How much surface each paint id owns. Areas are world cm^2 so surfaces of any scale add up. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintCoverage
{
	GENERATED_BODY()

	/** One entry per paint id; the PaintIdNone entry is the unpainted area. */
	UPROPERTY(BlueprintReadOnly, Category = "Paint")
	TArray<float> AreaByPaintId;

	UPROPERTY(BlueprintReadOnly, Category = "Paint")
	float TotalArea = 0.0f;

	FPaintCoverage();

	float GetFraction(uint8 PaintId) const;
	void Add(const FPaintCoverage& Other);

	/** "T0 25.0%  T1 8.0%  |  bare 67.0%" - painted ids only, then the unpainted remainder. */
	FString ToString() const;
};

/**
 * The gameplay layer's answer to "who owns this surface": a coarse voxel grid over the mesh's
 * local bounds where every voxel keeps one cell per face direction. Surface area is accumulated
 * into the cells once from the mesh triangles, and a splat marks the cells its stamp covers with
 * its paint id. Nothing here ever reads the render target; the grid and the brush are fed the
 * same FPaintLocalStamp, which is what keeps the two layers in agreement.
 *
 * Cells are (voxel, direction) pairs rather than voxels so a cube edge voxel can be painted on
 * its top without its side counting as painted, and so per-direction totals fall out for free.
 */
class MINTCHOCO_API FPaintCellGrid
{
public:
	/**
	 * Builds the surface cells from a triangle list in mesh local space. Indices is a flat
	 * triangle list into Positions; Normals may be empty, in which case winding decides which
	 * way a triangle faces. AreaScale converts local area to world area (uniform scale squared).
	 */
	void Build(
		const FBox& LocalBounds,
		float InCellSize,
		float AreaScale,
		TArrayView<const FVector3f> Positions,
		TArrayView<const FVector3f> Normals,
		TArrayView<const uint32> Indices);

	/**
	 * Builds from the triangles of one material slot of a static mesh, read from LOD 0's CPU
	 * copy of the vertex and index buffers. WorldCellSize is in cm; UniformScale is the mesh's
	 * world scale. Logs why and returns false when the mesh offers nothing to build from.
	 */
	bool BuildFromMesh(
		const UStaticMeshComponent& Mesh,
		int32 MaterialSlot,
		float WorldCellSize,
		float UniformScale,
		const FBox& LocalBounds);

	/**
	 * Paints every cell whose voxel center lies inside the stamp body and whose direction does
	 * not face away from the splat. CoreFraction is the part of the stamp radius that counts as
	 * covered: the brush's main blob spans half the radius, its satellites almost all of it.
	 * Returns the number of cells that changed id.
	 */
	int32 Mark(const FPaintLocalStamp& Stamp, uint8 PaintId, float CoreFraction);

	void ClearPaint();

	FPaintCoverage GetCoverage() const;
	FPaintCoverage GetCoverage(EPaintFaceDirection Direction) const;

	bool IsBuilt() const { return SurfaceCellCount > 0; }
	int32 GetSurfaceCellCount() const { return SurfaceCellCount; }
	float GetCellSize() const { return CellSize; }
	const FIntVector& GetDims() const { return Dims; }

	/** SurfaceCenter is the area-weighted center of the surface inside the cell, not the voxel center. */
	void ForEachSurfaceCell(
		TFunctionRef<void(const FVector& SurfaceCenter, EPaintFaceDirection Direction, uint8 PaintId, float Area)> Visitor) const;

private:
	/** A triangle clipped by up to six voxel planes has at most nine corners. */
	using FClipPolygon = TArray<FVector, TInlineAllocator<12>>;

	int32 VoxelIndex(const FIntVector& Voxel) const;
	FIntVector VoxelOf(const FVector& LocalPosition) const;
	FVector VoxelCenter(const FIntVector& Voxel) const;
	void DepositClipped(const FClipPolygon& Polygon, int32 Axis, int32 Direction, float AreaScale);

	FVector Origin = FVector::ZeroVector;
	float CellSize = 25.0f;
	FIntVector Dims = FIntVector::ZeroValue;

	/** All indexed by voxel * PaintFaceDirectionCount + direction. A zero area means no surface. */
	TArray<uint8> Ids;
	TArray<float> Areas;

	/** Where the surface actually sits inside the voxel; a voxel straddles its surface, so its center does not. */
	TArray<FVector3f> SurfaceCenters;

	/** Kept incrementally so a coverage query never walks the cells. */
	float Totals[PaintFaceDirectionCount][PaintIdCount] = {};
	float TotalArea = 0.0f;
	int32 SurfaceCellCount = 0;
};
