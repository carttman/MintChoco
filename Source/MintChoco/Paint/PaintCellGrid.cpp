#include "Paint/PaintCellGrid.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"

#include "Paint/PaintLog.h"

FVector PaintFaceDirectionVector(EPaintFaceDirection Direction)
{
	switch (Direction)
	{
	case EPaintFaceDirection::Front: return FVector::ForwardVector;
	case EPaintFaceDirection::Back: return FVector::BackwardVector;
	case EPaintFaceDirection::Right: return FVector::RightVector;
	case EPaintFaceDirection::Left: return FVector::LeftVector;
	case EPaintFaceDirection::Up: return FVector::UpVector;
	case EPaintFaceDirection::Down: return FVector::DownVector;
	}
	return FVector::UpVector;
}

EPaintFaceDirection ClassifyPaintFaceDirection(const FVector& Normal)
{
	const FVector Abs = Normal.GetAbs();
	if (Abs.X >= Abs.Y && Abs.X >= Abs.Z)
	{
		return Normal.X >= 0.0 ? EPaintFaceDirection::Front : EPaintFaceDirection::Back;
	}
	if (Abs.Y >= Abs.Z)
	{
		return Normal.Y >= 0.0 ? EPaintFaceDirection::Right : EPaintFaceDirection::Left;
	}
	return Normal.Z >= 0.0 ? EPaintFaceDirection::Up : EPaintFaceDirection::Down;
}

FPaintCoverage::FPaintCoverage()
{
	AreaByPaintId.Init(0.0f, PaintIdCount);
}

float FPaintCoverage::GetFraction(uint8 PaintId) const
{
	if (PaintId >= AreaByPaintId.Num() || TotalArea <= 0.0f)
	{
		return 0.0f;
	}
	return AreaByPaintId[PaintId] / TotalArea;
}

void FPaintCoverage::Add(const FPaintCoverage& Other)
{
	for (int32 Id = 0; Id < PaintIdCount; ++Id)
	{
		AreaByPaintId[Id] += Other.AreaByPaintId[Id];
	}
	TotalArea += Other.TotalArea;
}

FString FPaintCoverage::ToString() const
{
	FString Result;
	for (int32 Id = 0; Id < PaintIdNone; ++Id)
	{
		if (AreaByPaintId[Id] > 0.0f)
		{
			Result += FString::Printf(TEXT("T%d %.1f%%  "), Id, GetFraction(Id) * 100.0f);
		}
	}
	Result += FString::Printf(
		TEXT("%sbare %.1f%%"),
		Result.IsEmpty() ? TEXT("") : TEXT("|  "),
		GetFraction(PaintIdNone) * 100.0f);
	return Result;
}

void FPaintCellGrid::Build(
	const FBox& LocalBounds,
	float InCellSize,
	float AreaScale,
	TArrayView<const FVector3f> Positions,
	TArrayView<const FVector3f> Normals,
	TArrayView<const uint32> Indices)
{
	// A voxel cap keeps a huge mesh with a tiny cell size from allocating without bound; the
	// cell size doubles until the grid fits, which the log reports.
	constexpr int64 MaxVoxels = int64(1) << 20;

	Origin = LocalBounds.Min;
	CellSize = FMath::Max(InCellSize, UE_KINDA_SMALL_NUMBER);
	const FVector Size = LocalBounds.GetSize();
	const auto ComputeDims = [&Size](float Cell)
	{
		return FIntVector(
			FMath::Max(1, FMath::CeilToInt(Size.X / Cell)),
			FMath::Max(1, FMath::CeilToInt(Size.Y / Cell)),
			FMath::Max(1, FMath::CeilToInt(Size.Z / Cell)));
	};
	Dims = ComputeDims(CellSize);
	while (int64(Dims.X) * Dims.Y * Dims.Z > MaxVoxels)
	{
		CellSize *= 2.0f;
		Dims = ComputeDims(CellSize);
		UE_LOG(LogPaint, Warning, TEXT("Paint cell grid: too many voxels, cell size raised to %.1f."), CellSize);
	}

	const int32 CellCount = Dims.X * Dims.Y * Dims.Z * PaintFaceDirectionCount;
	Ids.Init(PaintIdNone, CellCount);
	Areas.Init(0.0f, CellCount);
	SurfaceCenters.Init(FVector3f::ZeroVector, CellCount);
	FMemory::Memzero(Totals);
	TotalArea = 0.0f;
	SurfaceCellCount = 0;

	const bool bHasNormals = Normals.Num() == Positions.Num();

	for (int32 First = 0; First + 2 < Indices.Num(); First += 3)
	{
		const uint32 I0 = Indices[First];
		const uint32 I1 = Indices[First + 1];
		const uint32 I2 = Indices[First + 2];
		const uint32 VertexCount = static_cast<uint32>(Positions.Num());
		if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
		{
			continue;
		}

		const FVector A(Positions[I0]);
		const FVector B(Positions[I1]);
		const FVector C(Positions[I2]);
		const FVector Cross = FVector::CrossProduct(B - A, C - A);
		const double DoubleArea = Cross.Size();
		if (DoubleArea <= UE_DOUBLE_SMALL_NUMBER)
		{
			continue;
		}

		// The geometric normal is exact per face but its sign depends on the winding convention;
		// the vertex normals settle that without assuming one.
		FVector FaceNormal = Cross / DoubleArea;
		if (bHasNormals)
		{
			const FVector VertexNormal = FVector(Normals[I0]) + FVector(Normals[I1]) + FVector(Normals[I2]);
			if (FVector::DotProduct(FaceNormal, VertexNormal) < 0.0)
			{
				FaceNormal = -FaceNormal;
			}
		}
		const int32 Direction = static_cast<int32>(ClassifyPaintFaceDirection(FaceNormal));

		// Exact rather than sampled: the triangle is clipped to every voxel it crosses and each
		// piece's true area lands in that voxel, so a cell border never shifts area to a neighbour.
		const FClipPolygon Triangle = {A, B, C};
		DepositClipped(Triangle, 0, Direction, AreaScale);
	}

	for (int32 Cell = 0; Cell < CellCount; ++Cell)
	{
		if (Areas[Cell] > 0.0f)
		{
			SurfaceCenters[Cell] /= Areas[Cell];
		}
	}
}

namespace
{
	/** Sutherland-Hodgman against one axis-aligned plane. The polygons stay convex, so this is exact. */
	void ClipToHalfSpace(const TArray<FVector, TInlineAllocator<12>>& In, TArray<FVector, TInlineAllocator<12>>& Out, int32 Axis, double Plane, bool bKeepAbove)
	{
		Out.Reset();
		const int32 Num = In.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FVector& P = In[Index];
			const FVector& Q = In[(Index + 1) % Num];
			const double DistP = bKeepAbove ? P[Axis] - Plane : Plane - P[Axis];
			const double DistQ = bKeepAbove ? Q[Axis] - Plane : Plane - Q[Axis];
			if (DistP >= 0.0)
			{
				Out.Add(P);
			}
			if ((DistP >= 0.0) != (DistQ >= 0.0))
			{
				Out.Add(P + (Q - P) * (DistP / (DistP - DistQ)));
			}
		}
	}
}

void FPaintCellGrid::DepositClipped(const FClipPolygon& Polygon, int32 Axis, int32 Direction, float AreaScale)
{
	if (Polygon.Num() < 3)
	{
		return;
	}

	if (Axis == 3)
	{
		// Fully inside one voxel: fan-triangulate for the area and the area-weighted centroid.
		FVector CrossSum = FVector::ZeroVector;
		FVector CentroidSum = FVector::ZeroVector;
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			const FVector Cross = FVector::CrossProduct(Polygon[Index] - Polygon[0], Polygon[Index + 1] - Polygon[0]);
			CrossSum += Cross;
			CentroidSum += (Polygon[0] + Polygon[Index] + Polygon[Index + 1]) * (Cross.Size() / 3.0);
		}
		const double DoubleArea = CrossSum.Size();
		if (DoubleArea <= UE_DOUBLE_SMALL_NUMBER)
		{
			return;
		}
		const float Area = static_cast<float>(0.5 * DoubleArea) * AreaScale;
		const FVector Centroid = CentroidSum / DoubleArea;

		const int32 Cell = VoxelIndex(VoxelOf(Centroid)) * PaintFaceDirectionCount + Direction;
		if (Areas[Cell] <= 0.0f)
		{
			++SurfaceCellCount;
		}
		Areas[Cell] += Area;
		SurfaceCenters[Cell] += FVector3f(Centroid) * Area;
		Totals[Direction][PaintIdNone] += Area;
		TotalArea += Area;
		return;
	}

	double Lo = TNumericLimits<double>::Max();
	double Hi = TNumericLimits<double>::Lowest();
	for (const FVector& P : Polygon)
	{
		Lo = FMath::Min(Lo, P[Axis]);
		Hi = FMath::Max(Hi, P[Axis]);
	}
	const int32 Last = Dims[Axis] - 1;
	const int32 FirstVoxel = FMath::Clamp(FMath::FloorToInt((Lo - Origin[Axis]) / CellSize), 0, Last);
	const int32 LastVoxel = FMath::Clamp(FMath::FloorToInt((Hi - Origin[Axis]) / CellSize), 0, Last);

	FClipPolygon Above;
	FClipPolygon Slab;
	for (int32 Voxel = FirstVoxel; Voxel <= LastVoxel; ++Voxel)
	{
		const double Min = Origin[Axis] + Voxel * CellSize;
		// The outermost voxels keep whatever pokes past the bounds, so a triangle on the very
		// edge of the mesh never loses area to clamping.
		ClipToHalfSpace(Polygon, Above, Axis, Voxel == 0 ? Lo : Min, true);
		ClipToHalfSpace(Above, Slab, Axis, Voxel == Last ? Hi : Min + CellSize, false);
		DepositClipped(Slab, Axis + 1, Direction, AreaScale);
	}
}

bool FPaintCellGrid::BuildFromMesh(
	const UStaticMeshComponent& Mesh,
	int32 MaterialSlot,
	float WorldCellSize,
	float UniformScale,
	const FBox& LocalBounds)
{
	const FString Owner = Mesh.GetReadableName();
	const UStaticMesh* const Asset = Mesh.GetStaticMesh();
	const FStaticMeshRenderData* const RenderData = Asset ? Asset->GetRenderData() : nullptr;
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no render data on the mesh, coverage disabled."), *Owner);
		return false;
	}

	// LOD 0 is the Nanite fallback on a Nanite mesh, which is plenty for cells this coarse. In a
	// cooked build these buffers only exist on the CPU if the mesh asset has Allow CPU Access on.
	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	const FIndexArrayView IndexView = LOD.IndexBuffer.GetArrayView();
	const uint32 VertexCount = PositionBuffer.GetNumVertices();
	if (VertexCount == 0 || IndexView.Num() == 0)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: mesh geometry is not CPU-readable (enable Allow CPU Access on %s), coverage disabled."),
			*Owner, *GetNameSafe(Asset));
		return false;
	}

	// Only the slot that shows paint counts; a second material on the mesh is not paintable.
	TArray<uint32> Indices;
	for (const FStaticMeshSection& Section : LOD.Sections)
	{
		const int32 End = Section.FirstIndex + Section.NumTriangles * 3;
		if (Section.MaterialIndex != MaterialSlot || End > IndexView.Num())
		{
			continue;
		}
		Indices.Reserve(Indices.Num() + Section.NumTriangles * 3);
		for (int32 Index = Section.FirstIndex; Index < End; ++Index)
		{
			Indices.Add(IndexView[Index]);
		}
	}
	if (Indices.IsEmpty())
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: material slot %d has no triangles, coverage disabled."), *Owner, MaterialSlot);
		return false;
	}

	TArray<FVector3f> Normals;
	if (VertexBuffer.GetNumVertices() == VertexCount)
	{
		Normals.SetNumUninitialized(VertexCount);
		for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
		{
			Normals[Vertex] = FVector3f(VertexBuffer.VertexTangentZ(Vertex));
		}
	}
	const TArrayView<const FVector3f> Positions(
		static_cast<const FVector3f*>(PositionBuffer.GetVertexData()), VertexCount);

	Build(LocalBounds, WorldCellSize / UniformScale, UniformScale * UniformScale, Positions, Normals, Indices);
	return IsBuilt();
}

int32 FPaintCellGrid::Mark(const FPaintLocalStamp& Stamp, uint8 PaintId, float CoreFraction)
{
	if (!IsBuilt() || PaintId >= PaintIdCount)
	{
		return 0;
	}

	// The brush body is the ellipsoid with semi-axes (f R S, f R, R): f of the radius across the
	// surface (the stamp's main disc is f = 0.5 wide), but the full radius along the normal,
	// because the stamp shader shrinks its disc with sqrt(1 - (n / R)^2).
	const float SurfaceRadius = FMath::Max(Stamp.Radius * CoreFraction, UE_KINDA_SMALL_NUMBER);
	const float StretchedRadius = SurfaceRadius * FMath::Max(Stamp.Stretch, 1.0f);
	const float DepthRadius = FMath::Max(Stamp.Radius, UE_KINDA_SMALL_NUMBER);
	const float Reach = FMath::Max3(SurfaceRadius, StretchedRadius, DepthRadius);

	const FIntVector Min = VoxelOf(Stamp.Center - FVector(Reach));
	const FIntVector Max = VoxelOf(Stamp.Center + FVector(Reach));

	int32 Changed = 0;
	for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				const FIntVector Voxel(X, Y, Z);
				const FVector D = VoxelCenter(Voxel) - Stamp.Center;
				const double U = FVector::DotProduct(D, Stamp.AxisU) / StretchedRadius;
				const double V = FVector::DotProduct(D, Stamp.AxisV) / SurfaceRadius;
				const double Nn = FVector::DotProduct(D, Stamp.Normal) / DepthRadius;
				if (U * U + V * V + Nn * Nn > 1.0)
				{
					continue;
				}

				const int32 Base = VoxelIndex(Voxel) * PaintFaceDirectionCount;
				for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
				{
					const int32 Cell = Base + Direction;
					if (Areas[Cell] <= 0.0f || Ids[Cell] == PaintId)
					{
						continue;
					}
					// The stamp ellipsoid reaches through thin geometry, and the render target does
					// paint the far side. Ownership does not follow: a face turned away from the
					// splat stays as it was.
					const FVector FaceNormal = PaintFaceDirectionVector(static_cast<EPaintFaceDirection>(Direction));
					if (FVector::DotProduct(FaceNormal, Stamp.Normal) < -UE_KINDA_SMALL_NUMBER)
					{
						continue;
					}

					Totals[Direction][Ids[Cell]] -= Areas[Cell];
					Totals[Direction][PaintId] += Areas[Cell];
					Ids[Cell] = PaintId;
					++Changed;
				}
			}
		}
	}
	return Changed;
}

void FPaintCellGrid::ClearPaint()
{
	FMemory::Memzero(Totals);
	for (int32 Cell = 0; Cell < Ids.Num(); ++Cell)
	{
		Ids[Cell] = PaintIdNone;
		Totals[Cell % PaintFaceDirectionCount][PaintIdNone] += Areas[Cell];
	}
}

FPaintCoverage FPaintCellGrid::GetCoverage() const
{
	FPaintCoverage Coverage;
	for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
	{
		for (int32 Id = 0; Id < PaintIdCount; ++Id)
		{
			Coverage.AreaByPaintId[Id] += Totals[Direction][Id];
		}
	}
	Coverage.TotalArea = TotalArea;
	return Coverage;
}

FPaintCoverage FPaintCellGrid::GetCoverage(EPaintFaceDirection Direction) const
{
	FPaintCoverage Coverage;
	const int32 Row = static_cast<int32>(Direction);
	for (int32 Id = 0; Id < PaintIdCount; ++Id)
	{
		Coverage.AreaByPaintId[Id] = Totals[Row][Id];
		Coverage.TotalArea += Totals[Row][Id];
	}
	return Coverage;
}

void FPaintCellGrid::ForEachSurfaceCell(
	TFunctionRef<void(const FVector& SurfaceCenter, EPaintFaceDirection Direction, uint8 PaintId, float Area)> Visitor) const
{
	for (int32 Cell = 0; Cell < Areas.Num(); ++Cell)
	{
		if (Areas[Cell] > 0.0f)
		{
			Visitor(
				FVector(SurfaceCenters[Cell]),
				static_cast<EPaintFaceDirection>(Cell % PaintFaceDirectionCount),
				Ids[Cell],
				Areas[Cell]);
		}
	}
}

int32 FPaintCellGrid::VoxelIndex(const FIntVector& Voxel) const
{
	return (Voxel.Z * Dims.Y + Voxel.Y) * Dims.X + Voxel.X;
}

FIntVector FPaintCellGrid::VoxelOf(const FVector& LocalPosition) const
{
	const FVector Scaled = (LocalPosition - Origin) / CellSize;
	return FIntVector(
		FMath::Clamp(FMath::FloorToInt(Scaled.X), 0, Dims.X - 1),
		FMath::Clamp(FMath::FloorToInt(Scaled.Y), 0, Dims.Y - 1),
		FMath::Clamp(FMath::FloorToInt(Scaled.Z), 0, Dims.Z - 1));
}

FVector FPaintCellGrid::VoxelCenter(const FIntVector& Voxel) const
{
	return Origin + (FVector(Voxel) + 0.5) * CellSize;
}
