#include "Paint/PaintCellGrid.h"

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

const TCHAR* PaintFaceDirectionName(EPaintFaceDirection Direction)
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

FColor PaintIdDebugColor(uint8 PaintId)
{
	static const FColor Palette[PaintIdCount] = {
		FColor::Red, FColor::Blue, FColor::Yellow, FColor::Green,
		FColor::Cyan, FColor::Magenta, FColor::Orange, FColor::Silver
	};
	return Palette[FMath::Min<int32>(PaintId, PaintIdCount - 1)];
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
		UE_LOG(LogTemp, Warning, TEXT("Paint cell grid: too many voxels, cell size raised to %.1f."), CellSize);
	}

	const int32 CellCount = Dims.X * Dims.Y * Dims.Z * PaintFaceDirectionCount;
	Ids.Init(PaintIdNone, CellCount);
	Areas.Init(0.0f, CellCount);
	SurfaceCenters.Init(FVector3f::ZeroVector, CellCount);
	FMemory::Memzero(Totals);
	TotalArea = 0.0f;
	SurfaceCellCount = 0;

	// Sampling a few points per cell along each triangle is what makes the area estimate
	// converge without a triangle/box clipping routine; each triangle's total is exact.
	const float Stride = CellSize / 4.0f;
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

		const double MaxEdge = FMath::Max3((B - A).Size(), (C - B).Size(), (A - C).Size());
		const int32 N = FMath::Max(1, FMath::CeilToInt(MaxEdge / Stride));
		const int32 SampleCount = (N + 1) * (N + 2) / 2;
		const float SampleArea = static_cast<float>(0.5 * DoubleArea) * AreaScale / SampleCount;

		for (int32 I = 0; I <= N; ++I)
		{
			for (int32 J = 0; J <= N - I; ++J)
			{
				const double U = double(I) / N;
				const double V = double(J) / N;
				const FVector Point = A + (B - A) * U + (C - A) * V;
				const int32 Cell = VoxelIndex(VoxelOf(Point)) * PaintFaceDirectionCount + Direction;
				if (Areas[Cell] <= 0.0f)
				{
					++SurfaceCellCount;
				}
				Areas[Cell] += SampleArea;
				SurfaceCenters[Cell] += FVector3f(Point) * SampleArea;
				Totals[Direction][PaintIdNone] += SampleArea;
				TotalArea += SampleArea;
			}
		}
	}

	for (int32 Cell = 0; Cell < CellCount; ++Cell)
	{
		if (Areas[Cell] > 0.0f)
		{
			SurfaceCenters[Cell] /= Areas[Cell];
		}
	}
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
