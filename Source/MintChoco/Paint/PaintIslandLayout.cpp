#include "Paint/PaintIslandLayout.h"

#include "Paint/PaintLog.h"

namespace
{
	struct FPendingIsland
	{
		FPaintIsland Island;
		int32 Width = 0;
		int32 Height = 0;
	};

	/** Shelf packing: rows of islands sorted tallest first. Exact enough for six rectangles. */
	bool TryPack(TArray<FPendingIsland>& Pending, int32 AtlasSize)
	{
		Pending.StableSort([](const FPendingIsland& A, const FPendingIsland& B) { return A.Height > B.Height; });

		int32 X = 0;
		int32 Y = 0;
		int32 ShelfHeight = 0;
		for (FPendingIsland& Entry : Pending)
		{
			if (X + Entry.Width > AtlasSize)
			{
				Y += ShelfHeight;
				X = 0;
				ShelfHeight = 0;
			}
			if (Y + Entry.Height > AtlasSize || Entry.Width > AtlasSize)
			{
				return false;
			}
			Entry.Island.Rect = FIntRect(X, Y, X + Entry.Width, Y + Entry.Height);
			X += Entry.Width;
			ShelfHeight = FMath::Max(ShelfHeight, Entry.Height);
		}
		return true;
	}
}

void FPaintIsland::PlaneAxes(int32 Axis, int32& OutAxisB, int32& OutAxisC)
{
	switch (Axis)
	{
	case 0:
		OutAxisB = 1;
		OutAxisC = 2;
		break;
	case 1:
		OutAxisB = 0;
		OutAxisC = 2;
		break;
	default:
		OutAxisB = 0;
		OutAxisC = 1;
		break;
	}
}

FVector4f FPaintIsland::ToShaderParam(int32 AtlasSize) const
{
	const float Inv = 1.0f / FMath::Max(AtlasSize, 1);
	return FVector4f(
		ContentOrigin.X * Inv,
		ContentOrigin.Y * Inv,
		static_cast<float>(ContentTexels.X) * Inv,
		static_cast<float>(ContentTexels.Y) * Inv);
}

FPaintIslandLayout FPaintIslandLayout::Build(
	const FBox& LocalBounds,
	const FVector& Scale3D,
	uint8 InEnabledDirections,
	float RequestedTexelCm,
	int32 PadTexels,
	int32 MinSize,
	int32 MaxSize)
{
	FPaintIslandLayout Layout;
	Layout.EnabledDirections = InEnabledDirections;
	Layout.TexelCm = FMath::Max(RequestedTexelCm, 0.01f);
	if (InEnabledDirections == 0)
	{
		return Layout;
	}

	MinSize = FMath::Max<int32>(FMath::RoundUpToPowerOfTwo(FMath::Max(MinSize, 1)), 1);
	MaxSize = FMath::Max<int32>(FMath::RoundUpToPowerOfTwo(FMath::Max(MaxSize, 1)), MinSize);
	// A gutter wider than the atlas can never fit, whatever the texel does.
	PadTexels = FMath::Clamp(PadTexels, 0, MaxSize / 8);

	const FVector WorldSize = LocalBounds.GetSize() * Scale3D.GetAbs();

	constexpr int32 MaxAttempts = 64;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		TArray<FPendingIsland> Pending;
		int64 TotalArea = 0;
		int32 MaxSide = 0;
		for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
		{
			const auto Face = static_cast<EPaintFaceDirection>(Direction);
			if (!(InEnabledDirections & PaintDirectionBit(Face)))
			{
				continue;
			}
			FPendingIsland& Entry = Pending.AddDefaulted_GetRef();
			FPaintIsland& Island = Entry.Island;
			Island.Direction = Face;
			Island.Axis = Direction / 2;
			Island.Sign = (Direction % 2 == 0) ? 1 : -1;
			FPaintIsland::PlaneAxes(Island.Axis, Island.AxisB, Island.AxisC);
			Island.ContentTexels = FVector2D(WorldSize[Island.AxisB], WorldSize[Island.AxisC]) / Layout.TexelCm;
			Entry.Width = FMath::Max(1, FMath::CeilToInt(Island.ContentTexels.X)) + 2 * PadTexels;
			Entry.Height = FMath::Max(1, FMath::CeilToInt(Island.ContentTexels.Y)) + 2 * PadTexels;
			TotalArea += int64(Entry.Width) * Entry.Height;
			MaxSide = FMath::Max3(MaxSide, Entry.Width, Entry.Height);
		}

		const int32 Needed = FMath::Max(MaxSide, FMath::CeilToInt(FMath::Sqrt(static_cast<double>(TotalArea))));
		int32 AtlasSize = FMath::Clamp<int32>(FMath::RoundUpToPowerOfTwo(FMath::Max(Needed, 1)), MinSize, MaxSize);
		for (; AtlasSize <= MaxSize; AtlasSize *= 2)
		{
			if (TryPack(Pending, AtlasSize))
			{
				Layout.AtlasSize = AtlasSize;
				Layout.Islands.Reset(Pending.Num());
				for (FPendingIsland& Entry : Pending)
				{
					Entry.Island.ContentOrigin = Entry.Island.Rect.Min + FIntPoint(PadTexels, PadTexels);
					Layout.Islands.Add(Entry.Island);
				}
				Layout.Islands.Sort([](const FPaintIsland& A, const FPaintIsland& B) { return A.Direction < B.Direction; });
				if (Layout.TexelCm > RequestedTexelCm * 1.001f)
				{
					UE_LOG(LogPaint, Log, TEXT("Paint atlas: %.2f cm texels requested, %.2f cm needed to fit %d islands into %d."),
						RequestedTexelCm, Layout.TexelCm, Pending.Num(), MaxSize);
				}
				return Layout;
			}
		}

		// Nothing fits even at MaxSize: coarsen towards the area that would, and a bit more so the
		// packing waste has room, then try again.
		const double Fill = static_cast<double>(TotalArea) / (0.85 * double(MaxSize) * MaxSize);
		Layout.TexelCm *= FMath::Max(1.05, FMath::Sqrt(Fill));
	}

	UE_LOG(LogPaint, Error, TEXT("Paint atlas: could not fit the islands into %d texels."), MaxSize);
	Layout.Islands.Reset();
	Layout.AtlasSize = 0;
	return Layout;
}

const FPaintIsland* FPaintIslandLayout::Find(EPaintFaceDirection Direction) const
{
	return Islands.FindByPredicate([Direction](const FPaintIsland& Island) { return Island.Direction == Direction; });
}

uint32 FPaintIslandLayout::ComputeHash() const
{
	return FCrc::StrCrc32(*ToString());
}

FString FPaintIslandLayout::ToString() const
{
	FString Result = FString::Printf(TEXT("mask %02x, %d texels, %.3f cm"), EnabledDirections, AtlasSize, TexelCm);
	for (const FPaintIsland& Island : Islands)
	{
		Result += FString::Printf(TEXT(" | %d:[%d,%d %dx%d] %.2fx%.2f"),
			static_cast<int32>(Island.Direction),
			Island.Rect.Min.X, Island.Rect.Min.Y, Island.Rect.Width(), Island.Rect.Height(),
			Island.ContentTexels.X, Island.ContentTexels.Y);
	}
	return Result;
}
