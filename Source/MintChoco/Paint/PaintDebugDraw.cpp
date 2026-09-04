#include "Paint/PaintDebugDraw.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace PaintDebug
{
	FColor IdColor(uint8 PaintId)
	{
		static const FColor Palette[PaintIdCount] = {
			FColor::Red, FColor::Blue, FColor::Yellow, FColor::Green,
			FColor::Cyan, FColor::Magenta, FColor::Orange, FColor::Silver
		};
		return Palette[FMath::Min<int32>(PaintId, PaintIdCount - 1)];
	}

	const TCHAR* FaceName(EPaintFaceDirection Direction)
	{
		switch (Direction)
		{
		case EPaintFaceDirection::Front: return TEXT("Front");
		case EPaintFaceDirection::Back: return TEXT("Back");
		case EPaintFaceDirection::Right: return TEXT("Right");
		case EPaintFaceDirection::Left: return TEXT("Left");
		case EPaintFaceDirection::Up: return TEXT("Up");
		case EPaintFaceDirection::Down: return TEXT("Down");
		}
		return TEXT("?");
	}

	void DrawCoverageText(
		const UWorld* World,
		const FTransform& MeshTransform,
		float UniformScale,
		const FBox& LocalBounds,
		const FPaintCellGrid& Grid,
		const FString& Label)
	{
#if ENABLE_DRAW_DEBUG
		const FVector LocalCenter = LocalBounds.GetCenter();
		const FVector LocalExtent = LocalBounds.GetExtent();

		// Duration 0 lives exactly one frame, so re-issuing every tick keeps the text current
		// without ever stacking stale copies.
		DrawDebugString(
			World, MeshTransform.TransformPosition(LocalCenter),
			FString::Printf(TEXT("%s  %s"), *Label, *Grid.GetCoverage().ToString()),
			nullptr, FColor::White, 0.0f, true);

		constexpr float MarginWorld = 20.0f;
		for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
		{
			const auto Face = static_cast<EPaintFaceDirection>(Direction);
			const FPaintCoverage Coverage = Grid.GetCoverage(Face);
			if (Coverage.TotalArea <= 0.0f)
			{
				continue;
			}
			const FVector Axis = PaintFaceDirectionVector(Face);
			const double Reach = FVector::DotProduct(Axis.GetAbs(), LocalExtent) + MarginWorld / UniformScale;
			DrawDebugString(
				World, MeshTransform.TransformPosition(LocalCenter + Axis * Reach),
				FString::Printf(TEXT("%s  %s"), FaceName(Face), *Coverage.ToString()),
				nullptr, IdColor(PaintIdNone), 0.0f, true);
		}
#endif
	}

	void DrawCells(
		const UWorld* World,
		const FTransform& MeshTransform,
		float UniformScale,
		const FPaintCellGrid& Grid)
	{
#if ENABLE_DRAW_DEBUG
		// A cell is a patch of surface, so it is drawn as a slab centered on that surface, facing
		// its direction, rather than as the voxel it lives in (which straddles the surface). Thick
		// enough to stand clear of the displaced paint, which would otherwise swallow a thin one.
		const float HalfCell = Grid.GetCellSize() * UniformScale * 0.45f;
		const FVector Extent(HalfCell, HalfCell, HalfCell * 0.5f);
		Grid.ForEachSurfaceCell([&](const FVector& SurfaceCenter, EPaintFaceDirection Direction, uint8 PaintId, float)
		{
			const bool bPainted = PaintId != PaintIdNone;
			const FQuat FaceRotation = FRotationMatrix::MakeFromZ(PaintFaceDirectionVector(Direction)).ToQuat();
			DrawDebugBox(
				World, MeshTransform.TransformPosition(SurfaceCenter), Extent, MeshTransform.GetRotation() * FaceRotation,
				bPainted ? IdColor(PaintId) : FColor(70, 70, 70),
				false, 0.0f, 0, bPainted ? 1.0f : 0.0f);
		});
#endif
	}
}
