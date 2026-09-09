#include "Misc/AutomationTest.h"

#include "Paint/PaintAtlasBaker.h"
#include "Paint/PaintIslandLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float TexelCm = 0.5f;
	constexpr int32 Pad = 8;
	constexpr int32 MinSize = 256;
	constexpr int32 MaxSize = 2048;

	/** A 100 cm cube centered on the origin, outward corner normals. */
	struct FAtlasTestCube
	{
		FPaintAtlasBakeInput Input;

		FAtlasTestCube()
		{
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector3f P(
					(Corner & 1) ? 50.0f : -50.0f,
					(Corner & 2) ? 50.0f : -50.0f,
					(Corner & 4) ? 50.0f : -50.0f);
				Input.Positions.Add(P);
				Input.Normals.Add(P.GetSafeNormal());
			}
			const int32 Faces[6][4] = {
				{1, 3, 7, 5}, {0, 4, 6, 2},
				{2, 6, 7, 3}, {0, 1, 5, 4},
				{4, 5, 7, 6}, {0, 2, 3, 1},
			};
			for (const auto& Face : Faces)
			{
				Input.Indices.Append({uint32(Face[0]), uint32(Face[1]), uint32(Face[2])});
				Input.Indices.Append({uint32(Face[0]), uint32(Face[2]), uint32(Face[3])});
			}
			Input.LocalBounds = FBox(FVector(-50.0), FVector(50.0));
		}

		void Layout(uint8 Mask)
		{
			Input.Layout = FPaintIslandLayout::Build(Input.LocalBounds, FVector::OneVector, Mask, TexelCm, Pad, MinSize, MaxSize);
		}
	};

	/** Two horizontal quads over the same footprint: one at z = Low facing down, one at z = High facing up. */
	FPaintAtlasBakeInput MakeStackedQuads(float Low, float High)
	{
		FPaintAtlasBakeInput Input;
		const float Z[2] = {Low, High};
		for (int32 Quad = 0; Quad < 2; ++Quad)
		{
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Input.Positions.Add(FVector3f((Corner & 1) ? 50.0f : -50.0f, (Corner & 2) ? 50.0f : -50.0f, Z[Quad]));
				Input.Normals.Add(Quad == 0 ? FVector3f(0, 0, -1) : FVector3f(0, 0, 1));
			}
			const uint32 Base = Quad * 4;
			Input.Indices.Append({Base, Base + 1, Base + 3, Base, Base + 3, Base + 2});
		}
		Input.LocalBounds = FBox(FVector(-50.0, -50.0, Low), FVector(50.0, 50.0, High));
		return Input;
	}

	int32 TexelIndex(const FPaintIsland& Island, const FVector& Normalized, int32 AtlasSize)
	{
		const FVector2D Texel = Island.ProjectNormalized(Normalized);
		return FMath::FloorToInt(Texel.Y) * AtlasSize + FMath::FloorToInt(Texel.X);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAtlasLayoutSingleIslandTest,
	"MintChoco.Paint.Atlas.LayoutSingleIsland",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAtlasLayoutSingleIslandTest::RunTest(const FString& Parameters)
{
	FAtlasTestCube Cube;
	Cube.Layout(PaintDirectionBit(EPaintFaceDirection::Up));
	const FPaintIslandLayout& Layout = Cube.Input.Layout;

	TestEqual(TEXT("one island"), Layout.Islands.Num(), 1);
	TestEqual(TEXT("texel kept"), Layout.TexelCm, TexelCm, 1e-5f);
	TestEqual(TEXT("216 texels round up to 256"), Layout.AtlasSize, 256);
	if (const FPaintIsland* Up = Layout.Find(EPaintFaceDirection::Up))
	{
		TestEqual(TEXT("projects along Z"), Up->Axis, 2);
		TestEqual(TEXT("keeps the top"), Up->Sign, 1);
		TestTrue(TEXT("rect"), Up->Rect == FIntRect(0, 0, 216, 216));
		TestTrue(TEXT("content origin"), Up->ContentOrigin == FIntPoint(Pad, Pad));
		TestTrue(TEXT("content texels"), Up->ContentTexels.Equals(FVector2D(200.0, 200.0), 1e-3));
		TestTrue(TEXT("shader param"), Up->ToShaderParam(256).Equals(FVector4f(8.0f / 256, 8.0f / 256, 200.0f / 256, 200.0f / 256), 1e-5f));
	}
	else
	{
		AddError(TEXT("Up island missing"));
	}
	TestNull(TEXT("disabled direction has no island"), Layout.Find(EPaintFaceDirection::Down));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAtlasLayoutAllSixTest,
	"MintChoco.Paint.Atlas.LayoutAllSixFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAtlasLayoutAllSixTest::RunTest(const FString& Parameters)
{
	// Six 216-texel islands need more than 512 texels on a side, so the texel has to grow.
	const FBox Bounds(FVector(-50.0), FVector(50.0));
	const FPaintIslandLayout Layout = FPaintIslandLayout::Build(Bounds, FVector::OneVector, PaintAllDirectionsMask, TexelCm, Pad, MinSize, 512);

	TestEqual(TEXT("six islands"), Layout.Islands.Num(), 6);
	TestTrue(TEXT("fits the cap"), Layout.AtlasSize > 0 && Layout.AtlasSize <= 512);
	TestTrue(TEXT("texel coarsened"), Layout.TexelCm > TexelCm);
	for (int32 A = 0; A < Layout.Islands.Num(); ++A)
	{
		const FIntRect& RectA = Layout.Islands[A].Rect;
		TestTrue(TEXT("inside the atlas"), RectA.Min.X >= 0 && RectA.Min.Y >= 0 && RectA.Max.X <= Layout.AtlasSize && RectA.Max.Y <= Layout.AtlasSize);
		for (int32 B = A + 1; B < Layout.Islands.Num(); ++B)
		{
			const FIntRect& RectB = Layout.Islands[B].Rect;
			const bool bOverlap = RectA.Min.X < RectB.Max.X && RectB.Min.X < RectA.Max.X && RectA.Min.Y < RectB.Max.Y && RectB.Min.Y < RectA.Max.Y;
			TestFalse(TEXT("islands do not overlap"), bOverlap);
		}
	}
	TestEqual(TEXT("hash is stable"), Layout.ComputeHash(),
		FPaintIslandLayout::Build(Bounds, FVector::OneVector, PaintAllDirectionsMask, TexelCm, Pad, MinSize, 512).ComputeHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAtlasRasterUnitCubeTest,
	"MintChoco.Paint.Atlas.RasterUnitCubeUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAtlasRasterUnitCubeTest::RunTest(const FString& Parameters)
{
	FAtlasTestCube Cube;
	Cube.Layout(PaintDirectionBit(EPaintFaceDirection::Up));
	const FPaintIsland& Up = Cube.Input.Layout.Islands[0];
	const int32 N = Cube.Input.Layout.AtlasSize;

	TArray<FVector4f> Positions;
	PaintAtlasBaker::Rasterize(Cube.Input, Positions);
	TestEqual(TEXT("atlas sized"), Positions.Num(), N * N);

	// The texel under the top's center holds the top's center, at the texel's own half-texel offset.
	const FVector4f& Center = Positions[TexelIndex(Up, FVector(0.5, 0.5, 1.0), N)];
	TestEqual(TEXT("center x"), Center.X, 0.5f, 0.01f);
	TestEqual(TEXT("center y"), Center.Y, 0.5f, 0.01f);
	TestEqual(TEXT("top face is z = 1"), Center.Z, 1.0f, 1e-3f);
	TestEqual(TEXT("covered"), Center.W, 1.0f, 1e-6f);

	int32 Uncovered = 0;
	for (int32 Y = Up.ContentOrigin.Y; Y < Up.ContentOrigin.Y + 200; ++Y)
	{
		for (int32 X = Up.ContentOrigin.X; X < Up.ContentOrigin.X + 200; ++X)
		{
			Uncovered += Positions[Y * N + X].W > 0.5f ? 0 : 1;
		}
	}
	TestEqual(TEXT("every content texel is covered"), Uncovered, 0);

	const FVector4f& Gutter = Positions[(Up.ContentOrigin.Y + 100) * N + Up.ContentOrigin.X - 1];
	TestEqual(TEXT("gutter is empty"), Gutter.W, 0.0f, 1e-6f);
	TestEqual(TEXT("gutter holds the sentinel"), Gutter.X, PaintAtlasBaker::EmptyPosition, 1e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAtlasDepthTest,
	"MintChoco.Paint.Atlas.DepthKeepsOutermost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAtlasDepthTest::RunTest(const FString& Parameters)
{
	FPaintAtlasBakeInput Input = MakeStackedQuads(0.0f, 50.0f);
	const uint8 Mask = PaintDirectionBit(EPaintFaceDirection::Up) | PaintDirectionBit(EPaintFaceDirection::Down);
	Input.Layout = FPaintIslandLayout::Build(Input.LocalBounds, FVector::OneVector, Mask, TexelCm, Pad, MinSize, MaxSize);
	const int32 N = Input.Layout.AtlasSize;

	TArray<FVector4f> Positions;
	PaintAtlasBaker::Rasterize(Input, Positions);

	// Seen from above the upper quad wins; seen from below the lower one, which is the only one
	// facing that way. Normalized z: 0.5 for z = 0, 1.0 for z = 50 over bounds 0..50.
	const FPaintIsland* Up = Input.Layout.Find(EPaintFaceDirection::Up);
	const FPaintIsland* Down = Input.Layout.Find(EPaintFaceDirection::Down);
	if (!Up || !Down)
	{
		AddError(TEXT("islands missing"));
		return false;
	}
	TestEqual(TEXT("up keeps the upper quad"), Positions[TexelIndex(*Up, FVector(0.5, 0.5, 1.0), N)].Z, 1.0f, 1e-3f);
	TestEqual(TEXT("down keeps the lower quad"), Positions[TexelIndex(*Down, FVector(0.5, 0.5, 0.0), N)].Z, 0.0f, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAtlasEdgeFadeTest,
	"MintChoco.Paint.Atlas.EdgeFade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAtlasEdgeFadeTest::RunTest(const FString& Parameters)
{
	FAtlasTestCube Cube;
	Cube.Layout(PaintDirectionBit(EPaintFaceDirection::Up));
	Cube.Input.EdgeFadeTexels = 8.0f;
	Cube.Input.EdgeFadeSeamFraction = 0.05f;
	const FPaintIsland& Up = Cube.Input.Layout.Islands[0];
	const int32 N = Cube.Input.Layout.AtlasSize;

	FPaintAtlasBakeOutput Output;
	PaintAtlasBaker::Bake(Cube.Input, Output);
	TestEqual(TEXT("covered texels"), Output.CoveredTexels, 200 * 200);

	const int32 Row = Up.ContentOrigin.Y + 100;
	TestEqual(TEXT("center is fully faded in"), Output.EdgeFade[Row * N + Up.ContentOrigin.X + 100], 255);
	TestEqual(TEXT("outermost content texel is an edge"), Output.EdgeFade[Row * N + Up.ContentOrigin.X], 0);
	TestEqual(TEXT("gutter is zero"), Output.EdgeFade[Row * N + Up.ContentOrigin.X - 1], 0);
	for (int32 Step = 1; Step <= 8; ++Step)
	{
		const uint8 Previous = Output.EdgeFade[Row * N + Up.ContentOrigin.X + Step - 1];
		const uint8 Current = Output.EdgeFade[Row * N + Up.ContentOrigin.X + Step];
		TestTrue(TEXT("fade ramps up from the edge"), Current >= Previous);
	}
	TestEqual(TEXT("ramp reaches full at the fade width"), Output.EdgeFade[Row * N + Up.ContentOrigin.X + 8], 255);

	// A step inside one island: the left half at z = 0, the right half at z = 50, both facing up.
	FPaintAtlasBakeInput Step;
	const float Z[2] = {0.0f, 50.0f};
	for (int32 Half = 0; Half < 2; ++Half)
	{
		const float X0 = Half == 0 ? -50.0f : 0.0f;
		const float X1 = Half == 0 ? 0.0f : 50.0f;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Step.Positions.Add(FVector3f((Corner & 1) ? X1 : X0, (Corner & 2) ? 50.0f : -50.0f, Z[Half]));
			Step.Normals.Add(FVector3f(0, 0, 1));
		}
		const uint32 Base = Half * 4;
		Step.Indices.Append({Base, Base + 1, Base + 3, Base, Base + 3, Base + 2});
	}
	Step.LocalBounds = FBox(FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, 50.0));
	Step.Layout = FPaintIslandLayout::Build(Step.LocalBounds, FVector::OneVector, PaintDirectionBit(EPaintFaceDirection::Up), TexelCm, Pad, MinSize, MaxSize);
	Step.EdgeFadeTexels = 8.0f;
	Step.EdgeFadeSeamFraction = 0.05f;

	FPaintAtlasBakeOutput StepOutput;
	PaintAtlasBaker::Bake(Step, StepOutput);
	const FPaintIsland& StepUp = Step.Layout.Islands[0];
	const int32 StepN = Step.Layout.AtlasSize;
	const int32 StepRow = StepUp.ContentOrigin.Y + 100;
	const int32 SeamX = StepUp.ContentOrigin.X + 100;
	TestEqual(TEXT("left of the step is an edge"), StepOutput.EdgeFade[StepRow * StepN + SeamX - 1], 0);
	TestEqual(TEXT("right of the step is an edge"), StepOutput.EdgeFade[StepRow * StepN + SeamX], 0);
	TestEqual(TEXT("away from the step the fade recovers"), StepOutput.EdgeFade[StepRow * StepN + SeamX + 40], 255);
	return true;
}

#endif
