#include "Paint/PaintAtlasBaker.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

#include "Paint/PaintMeshTriangles.h"

namespace
{
	double Cross2(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	UTexture2D* CreateAtlasTexture(int32 AtlasSize, EPixelFormat Format, TextureFilter Filter, const void* Data, int64 Bytes, FName Name)
	{
		UTexture2D* const Texture = UTexture2D::CreateTransient(AtlasSize, AtlasSize, Format, Name);
		if (!Texture)
		{
			return nullptr;
		}

		// CreateTransient allocates mip 0 but only creates the resource when handed image data;
		// filling the mip here keeps the sampler settings below in place before that happens.
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* const Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Destination, Data, Bytes);
		Mip.BulkData.Unlock();

		Texture->Filter = Filter;
		Texture->SRGB = false;
		Texture->NeverStream = true;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->UpdateResource();
		return Texture;
	}
}

bool PaintAtlasBaker::GatherMesh(const UStaticMeshComponent& Mesh, int32 MaterialSlot, FPaintAtlasBakeInput& Out)
{
	FPaintMeshTriangles Triangles;
	if (!GatherPaintMeshTriangles(Mesh, MaterialSlot, Triangles))
	{
		return false;
	}
	Out.Positions = MoveTemp(Triangles.Positions);
	Out.Normals = MoveTemp(Triangles.Normals);
	Out.Indices = MoveTemp(Triangles.Indices);
	return true;
}

void PaintAtlasBaker::Rasterize(const FPaintAtlasBakeInput& In, TArray<FVector4f>& OutPositions)
{
	const int32 AtlasSize = In.Layout.AtlasSize;
	OutPositions.Init(FVector4f(EmptyPosition, EmptyPosition, EmptyPosition, 0.0f), AtlasSize * AtlasSize);
	if (AtlasSize <= 0)
	{
		return;
	}

	TArray<double> Depth;
	Depth.Init(TNumericLimits<double>::Lowest(), AtlasSize * AtlasSize);

	const FVector BoundsMin = In.LocalBounds.Min;
	const FVector BoundsSize = In.LocalBounds.GetSize();
	const FVector InvSize(
		BoundsSize.X > UE_DOUBLE_SMALL_NUMBER ? 1.0 / BoundsSize.X : 0.0,
		BoundsSize.Y > UE_DOUBLE_SMALL_NUMBER ? 1.0 / BoundsSize.Y : 0.0,
		BoundsSize.Z > UE_DOUBLE_SMALL_NUMBER ? 1.0 / BoundsSize.Z : 0.0);
	const bool bHasNormals = In.Normals.Num() == In.Positions.Num();
	const uint32 VertexCount = static_cast<uint32>(In.Positions.Num());

	for (const FPaintIsland& Island : In.Layout.Islands)
	{
		for (int32 First = 0; First + 2 < In.Indices.Num(); First += 3)
		{
			const uint32 I[3] = {In.Indices[First], In.Indices[First + 1], In.Indices[First + 2]};
			if (I[0] >= VertexCount || I[1] >= VertexCount || I[2] >= VertexCount)
			{
				continue;
			}
			const FVector P[3] = {FVector(In.Positions[I[0]]), FVector(In.Positions[I[1]]), FVector(In.Positions[I[2]])};

			FVector FaceNormal = FVector::CrossProduct(P[1] - P[0], P[2] - P[0]);
			const double Length = FaceNormal.Size();
			if (Length <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			FaceNormal /= Length;
			if (bHasNormals)
			{
				const FVector VertexNormal = FVector(In.Normals[I[0]]) + FVector(In.Normals[I[1]]) + FVector(In.Normals[I[2]]);
				if (FVector::DotProduct(FaceNormal, VertexNormal) < 0.0)
				{
					FaceNormal = -FaceNormal;
				}
			}
			// A face turned away from the island's viewpoint is never the outermost surface of a
			// closed mesh, and the reader never assigns such a pixel to this island either.
			if (Island.Sign * FaceNormal[Island.Axis] < -1e-4)
			{
				continue;
			}

			FVector Normalized[3];
			FVector2D Texel[3];
			double VertexDepth[3];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Normalized[Corner] = (P[Corner] - BoundsMin) * InvSize;
				Texel[Corner] = Island.ProjectNormalized(Normalized[Corner]);
				VertexDepth[Corner] = Island.Sign * P[Corner][Island.Axis];
			}

			const double Area2 = Cross2(Texel[1] - Texel[0], Texel[2] - Texel[0]);
			if (FMath::Abs(Area2) < 1e-6)
			{
				continue;
			}

			const int32 X0 = FMath::Max(Island.Rect.Min.X, FMath::FloorToInt(FMath::Min3(Texel[0].X, Texel[1].X, Texel[2].X)));
			const int32 X1 = FMath::Min(Island.Rect.Max.X - 1, FMath::CeilToInt(FMath::Max3(Texel[0].X, Texel[1].X, Texel[2].X)));
			const int32 Y0 = FMath::Max(Island.Rect.Min.Y, FMath::FloorToInt(FMath::Min3(Texel[0].Y, Texel[1].Y, Texel[2].Y)));
			const int32 Y1 = FMath::Min(Island.Rect.Max.Y - 1, FMath::CeilToInt(FMath::Max3(Texel[0].Y, Texel[1].Y, Texel[2].Y)));

			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const FVector2D Center(X + 0.5, Y + 0.5);
					const double W0 = Cross2(Texel[2] - Texel[1], Center - Texel[1]) / Area2;
					const double W1 = Cross2(Texel[0] - Texel[2], Center - Texel[2]) / Area2;
					const double W2 = Cross2(Texel[1] - Texel[0], Center - Texel[0]) / Area2;
					constexpr double Slack = -1e-4;
					if (W0 < Slack || W1 < Slack || W2 < Slack)
					{
						continue;
					}

					const int32 Index = Y * AtlasSize + X;
					const double TexelDepth = W0 * VertexDepth[0] + W1 * VertexDepth[1] + W2 * VertexDepth[2];
					if (TexelDepth <= Depth[Index])
					{
						continue;
					}
					Depth[Index] = TexelDepth;
					const FVector Position = W0 * Normalized[0] + W1 * Normalized[1] + W2 * Normalized[2];
					OutPositions[Index] = FVector4f(FVector3f(Position), 1.0f);
				}
			}
		}
	}
}

void PaintAtlasBaker::ComputeEdgeFade(
	TArrayView<const FVector4f> Positions, int32 AtlasSize, float FadeTexels, float SeamFraction, TArray<uint8>& OutFade)
{
	const int32 Count = AtlasSize * AtlasSize;
	OutFade.Init(0, Count);
	if (Count == 0)
	{
		return;
	}

	const auto Covered = [&Positions](int32 Index) { return Positions[Index].W > 0.5f; };
	const float SeamSquared = SeamFraction * SeamFraction;
	const auto Seam = [&Positions, SeamSquared](int32 A, int32 B)
	{
		const FVector3f Delta(Positions[A].X - Positions[B].X, Positions[A].Y - Positions[B].Y, Positions[A].Z - Positions[B].Z);
		return Delta.SizeSquared() > SeamSquared;
	};

	constexpr int32 Unreached = TNumericLimits<int32>::Max();
	TArray<int32> Distance;
	Distance.Init(Unreached, Count);
	TArray<int32> Frontier;
	for (int32 Y = 0; Y < AtlasSize; ++Y)
	{
		for (int32 X = 0; X < AtlasSize; ++X)
		{
			const int32 Index = Y * AtlasSize + X;
			bool bEdge = !Covered(Index) || X == 0 || Y == 0 || X == AtlasSize - 1 || Y == AtlasSize - 1;
			if (!bEdge)
			{
				const int32 Neighbours[4] = {Index - 1, Index + 1, Index - AtlasSize, Index + AtlasSize};
				for (const int32 Neighbour : Neighbours)
				{
					if (!Covered(Neighbour) || Seam(Index, Neighbour))
					{
						bEdge = true;
						break;
					}
				}
			}
			if (bEdge)
			{
				Distance[Index] = 0;
				Frontier.Add(Index);
			}
		}
	}

	// Multi-source breadth-first rings: Chebyshev distance to the nearest edge, up to the fade width.
	const int32 Rings = FMath::Max(1, FMath::CeilToInt(FadeTexels));
	TArray<int32> Next;
	for (int32 Ring = 1; Ring <= Rings && !Frontier.IsEmpty(); ++Ring)
	{
		Next.Reset();
		for (const int32 Index : Frontier)
		{
			const int32 X = Index % AtlasSize;
			const int32 Y = Index / AtlasSize;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (NX < 0 || NY < 0 || NX >= AtlasSize || NY >= AtlasSize)
					{
						continue;
					}
					const int32 Neighbour = NY * AtlasSize + NX;
					if (Distance[Neighbour] == Unreached)
					{
						Distance[Neighbour] = Ring;
						Next.Add(Neighbour);
					}
				}
			}
		}
		Swap(Frontier, Next);
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Covered(Index))
		{
			continue;
		}
		const float Fade = Distance[Index] == Unreached ? 1.0f : FMath::Clamp(Distance[Index] / FadeTexels, 0.0f, 1.0f);
		OutFade[Index] = static_cast<uint8>(FMath::RoundToInt(255.0f * Fade));
	}
}

void PaintAtlasBaker::Bake(const FPaintAtlasBakeInput& In, FPaintAtlasBakeOutput& Out)
{
	TArray<FVector4f> Positions;
	Rasterize(In, Positions);
	ComputeEdgeFade(Positions, In.Layout.AtlasSize, In.EdgeFadeTexels, In.EdgeFadeSeamFraction, Out.EdgeFade);

	Out.CoveredTexels = 0;
	Out.Positions.SetNumUninitialized(Positions.Num());
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FVector4f& Position = Positions[Index];
		Out.Positions[Index] = FFloat16Color(FLinearColor(Position.X, Position.Y, Position.Z, Position.W));
		Out.CoveredTexels += Position.W > 0.5f ? 1 : 0;
	}
}

UTexture2D* PaintAtlasBaker::CreatePositionTexture(const FPaintAtlasBakeOutput& Out, int32 AtlasSize, FName Name)
{
	check(Out.Positions.Num() == AtlasSize * AtlasSize);
	// Nearest for the same reason as the id buffer: bilinear across an island boundary would blend
	// two unrelated surface positions into one that exists nowhere on the mesh.
	return CreateAtlasTexture(
		AtlasSize, PF_FloatRGBA, TF_Nearest, Out.Positions.GetData(), Out.Positions.Num() * sizeof(FFloat16Color), Name);
}

UTexture2D* PaintAtlasBaker::CreateEdgeFadeTexture(const FPaintAtlasBakeOutput& Out, int32 AtlasSize, FName Name)
{
	check(Out.EdgeFade.Num() == AtlasSize * AtlasSize);
	// A plain scalar, so unlike the positions it can be filtered by the sampler.
	return CreateAtlasTexture(AtlasSize, PF_G8, TF_Bilinear, Out.EdgeFade.GetData(), Out.EdgeFade.Num(), Name);
}
