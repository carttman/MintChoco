#include "Misc/AutomationTest.h"

#include "Engine/HitResult.h"

#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintCellGrid.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A 100 cm cube centered on the origin: 8 shared corners, 12 triangles, outward corner normals. */
	struct FUnitCube
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<uint32> Indices;
		FBox Bounds = FBox(FVector(-50.0), FVector(50.0));

		FUnitCube()
		{
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector3f P(
					(Corner & 1) ? 50.0f : -50.0f,
					(Corner & 2) ? 50.0f : -50.0f,
					(Corner & 4) ? 50.0f : -50.0f);
				Positions.Add(P);
				Normals.Add(P.GetSafeNormal());
			}
			// Winding is irrelevant to the grid (the vertex normals fix the sign), so each face is
			// simply two triangles over its four corners.
			const int32 Faces[6][4] = {
				{1, 3, 7, 5}, {0, 4, 6, 2}, // +X, -X
				{2, 6, 7, 3}, {0, 1, 5, 4}, // +Y, -Y
				{4, 5, 7, 6}, {0, 2, 3, 1}, // +Z, -Z
			};
			for (const auto& Face : Faces)
			{
				Indices.Append({uint32(Face[0]), uint32(Face[1]), uint32(Face[2])});
				Indices.Append({uint32(Face[0]), uint32(Face[2]), uint32(Face[3])});
			}
		}

		void Build(FPaintCellGrid& Grid, float CellSize) const
		{
			Grid.Build(Bounds, CellSize, 1.0f, Positions, Normals, Indices);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCellGridUnitCubeTest,
	"MintChoco.Paint.CellGrid.UnitCube",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCellGridUnitCubeTest::RunTest(const FString& Parameters)
{
	FPaintCellGrid Grid;
	FUnitCube().Build(Grid, 25.0f);

	TestEqual(TEXT("dims"), Grid.GetDims(), FIntVector(4, 4, 4));
	TestEqual(TEXT("surface cells: 6 faces of 4 x 4"), Grid.GetSurfaceCellCount(), 96);
	TestEqual(TEXT("total area"), Grid.GetCoverage().TotalArea, 60000.0f, 60.0f);
	for (int32 Direction = 0; Direction < PaintFaceDirectionCount; ++Direction)
	{
		const FPaintCoverage Face = Grid.GetCoverage(static_cast<EPaintFaceDirection>(Direction));
		TestEqual(FString::Printf(TEXT("face %d area"), Direction), Face.TotalArea, 10000.0f, 10.0f);
		TestEqual(FString::Printf(TEXT("face %d starts unpainted"), Direction), Face.GetFraction(PaintIdNone), 1.0f, 1e-4f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCellGridMarkTopTest,
	"MintChoco.Paint.CellGrid.MarkTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCellGridMarkTopTest::RunTest(const FString& Parameters)
{
	FPaintCellGrid Grid;
	FUnitCube().Build(Grid, 25.0f);

	FPaintLocalStamp Stamp;
	Stamp.Center = FVector(0.0, 0.0, 50.0);
	Stamp.Normal = FVector::UpVector;
	Stamp.AxisU = FVector::ForwardVector;
	Stamp.AxisV = FVector::RightVector;
	Stamp.Radius = 60.0f;
	Stamp.Stretch = 1.0f;

	// The stamp body spans half the radius across the surface: 30 cm reaches the four center
	// cells of the top (centers at +-12.5 diagonally, 17.7 cm out) but not the ring at +-37.5;
	// the full radius along the normal still stops short of every side and the bottom face.
	const int32 Changed = Grid.Mark(Stamp, 1, 0.5f);
	TestEqual(TEXT("cells painted"), Changed, 4);
	TestEqual(TEXT("top owned by team 1"), Grid.GetCoverage(EPaintFaceDirection::Up).GetFraction(1), 0.25f, 1e-3f);
	for (const EPaintFaceDirection Other : {EPaintFaceDirection::Front, EPaintFaceDirection::Back,
		EPaintFaceDirection::Right, EPaintFaceDirection::Left, EPaintFaceDirection::Down})
	{
		TestEqual(FString::Printf(TEXT("face %d untouched"), int32(Other)), Grid.GetCoverage(Other).GetFraction(1), 0.0f, 1e-6f);
	}
	TestEqual(TEXT("whole cube"), Grid.GetCoverage().GetFraction(1), 2500.0f / 60000.0f, 1e-4f);

	// Painting the none id is the eraser.
	TestEqual(TEXT("cells erased"), Grid.Mark(Stamp, PaintIdNone, 0.5f), 4);
	TestEqual(TEXT("top bare again"), Grid.GetCoverage(EPaintFaceDirection::Up).GetFraction(PaintIdNone), 1.0f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCellGridMaskAndScaleTest,
	"MintChoco.Paint.CellGrid.MaskAndScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCellGridMaskAndScaleTest::RunTest(const FString& Parameters)
{
	// The component builds in the scaled-local frame: positions carry the world scale, cells stay
	// world-sized, and classification multiplies the scale back in so a stretched face still reads
	// as its own axis. Only the enabled directions get cells.
	const FVector Scale(2.0, 1.0, 1.0);
	FUnitCube Cube;
	for (FVector3f& Position : Cube.Positions)
	{
		Position *= FVector3f(Scale);
	}
	const FBox Bounds(FVector(-100.0, -50.0, -50.0), FVector(100.0, 50.0, 50.0));
	const uint8 Mask = PaintDirectionBit(EPaintFaceDirection::Up) | PaintDirectionBit(EPaintFaceDirection::Front);

	FPaintCellGrid Grid;
	Grid.Build(Bounds, 25.0f, 1.0f, Cube.Positions, Cube.Normals, Cube.Indices, Mask, Scale);

	TestEqual(TEXT("dims"), Grid.GetDims(), FIntVector(8, 4, 4));
	TestEqual(TEXT("surface cells: top 8 x 4 plus front 4 x 4"), Grid.GetSurfaceCellCount(), 48);
	TestEqual(TEXT("total area is the two enabled faces"), Grid.GetCoverage().TotalArea, 30000.0f, 30.0f);
	TestEqual(TEXT("top area follows the stretch"), Grid.GetCoverage(EPaintFaceDirection::Up).TotalArea, 20000.0f, 20.0f);
	TestEqual(TEXT("front area"), Grid.GetCoverage(EPaintFaceDirection::Front).TotalArea, 10000.0f, 10.0f);
	for (const EPaintFaceDirection Disabled : {EPaintFaceDirection::Back, EPaintFaceDirection::Right,
		EPaintFaceDirection::Left, EPaintFaceDirection::Down})
	{
		TestEqual(FString::Printf(TEXT("face %d disabled"), int32(Disabled)), Grid.GetCoverage(Disabled).TotalArea, 0.0f, 1e-6f);
	}

	// A world-radius stamp reaches the same four center cells on the stretched top as on the unit cube.
	FPaintLocalStamp Stamp;
	Stamp.Center = FVector(0.0, 0.0, 50.0);
	Stamp.Normal = FVector::UpVector;
	Stamp.AxisU = FVector::ForwardVector;
	Stamp.AxisV = FVector::RightVector;
	Stamp.Radius = 60.0f;
	Stamp.Stretch = 1.0f;
	TestEqual(TEXT("cells painted"), Grid.Mark(Stamp, 1, 0.5f), 4);
	TestEqual(TEXT("top owned by team 1"), Grid.GetCoverage(EPaintFaceDirection::Up).GetFraction(1), 2500.0f / 20000.0f, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCellGridStarLockTest,
	"MintChoco.Paint.CellGrid.StarLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCellGridStarLockTest::RunTest(const FString& Parameters)
{
	FPaintCellGrid Grid;
	FUnitCube().Build(Grid, 25.0f);

	FPaintLocalStamp Stamp;
	Stamp.Center = FVector(0.0, 0.0, 50.0);
	Stamp.Normal = FVector::UpVector;
	Stamp.AxisU = FVector::ForwardVector;
	Stamp.AxisV = FVector::RightVector;
	Stamp.Radius = 60.0f;
	Stamp.Stretch = 1.0f;

	const auto CountCells = [&Grid](uint8 PaintId, uint8 StarGen)
	{
		int32 Count = 0;
		Grid.ForEachSurfaceCell([&](const FVector&, EPaintFaceDirection, uint8 Id, uint8 Gen, float)
		{
			if (Id == PaintId && Gen == StarGen)
			{
				++Count;
			}
		});
		return Count;
	};
	const auto TopFraction = [&Grid](uint8 PaintId)
	{
		return Grid.GetCoverage(EPaintFaceDirection::Up).GetFraction(PaintId);
	};

	// A speed-star trail: team 0, generation 1, on the four center cells of the top.
	FPaintLockGens Locks;
	TestEqual(TEXT("trail painted"), Grid.Mark(Stamp, 0, 1, Locks, 0.5f), 4);
	TestEqual(TEXT("cells carry the generation"), CountCells(0, 1), 4);

	// While that generation is locked, no other id takes the cells and the score stays put.
	Locks.Gen[0] = 1;
	TestEqual(TEXT("the enemy is blocked"), Grid.Mark(Stamp, 1, 0, Locks, 0.5f), 0);
	TestEqual(TEXT("team 0 keeps the top"), TopFraction(0), 0.25f, 1e-3f);
	TestEqual(TEXT("the eraser is blocked too"), Grid.Mark(Stamp, PaintIdNone, 0, Locks, 0.5f), 0);
	TestEqual(TEXT("an enemy star is blocked as well"), Grid.Mark(Stamp, 1, 7, Locks, 0.5f), 0);

	// The team's own plain paint passes over the trail without disturbing the locked generation.
	TestEqual(TEXT("same team changes no owner"), Grid.Mark(Stamp, 0, 0, Locks, 0.5f), 0);
	TestEqual(TEXT("the generation survives"), CountCells(0, 1), 4);

	// Once the lock moves on to a later generation, the old trail is plain paint again.
	Locks.Gen[0] = 2;
	TestEqual(TEXT("the enemy takes the old trail"), Grid.Mark(Stamp, 1, 0, Locks, 0.5f), 4);
	TestEqual(TEXT("team 1 owns the top"), TopFraction(1), 0.25f, 1e-3f);
	TestEqual(TEXT("team 0 lost it"), TopFraction(0), 0.0f, 1e-6f);

	// A star over the team's own plain paint takes no cell but stamps its generation on them.
	Locks.Gen[0] = 0;
	TestEqual(TEXT("own star changes no owner"), Grid.Mark(Stamp, 1, 3, Locks, 0.5f), 0);
	TestEqual(TEXT("but stamps the generation"), CountCells(1, 3), 4);

	// Unlocked, plain paint of the same team clears the generation again.
	TestEqual(TEXT("plain paint over a faded trail"), Grid.Mark(Stamp, 1, 0, Locks, 0.5f), 0);
	TestEqual(TEXT("generation cleared"), CountCells(1, 0), 4);

	// The eraser never carries a generation; the plain overload paints nothing but the id.
	TestEqual(TEXT("erased"), Grid.Mark(Stamp, PaintIdNone, 9, Locks, 0.5f), 4);
	TestEqual(TEXT("bare cells carry no generation"), CountCells(PaintIdNone, 0), 96);
	TestEqual(TEXT("plain overload paints"), Grid.Mark(Stamp, 2, 0.5f), 4);
	TestEqual(TEXT("plain overload carries no generation"), CountCells(2, 0), 4);

	Grid.ClearPaint();
	TestEqual(TEXT("clear drops every generation"), CountCells(PaintIdNone, 0), 96);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintBrushBuildSplatTest,
	"MintChoco.Paint.Brush.BuildSplatDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintBrushBuildSplatTest::RunTest(const FString& Parameters)
{
	const UPaintBrushProfile* const Profile = NewObject<UPaintBrushProfile>();

	FHitResult Hit;
	Hit.ImpactPoint = FVector(100.0, 200.0, 300.0);
	Hit.ImpactNormal = FVector::UpVector;

	// Head-on: the stamp is round, so its rotation comes from the seed.
	const FVector HeadOn = FVector::DownVector * 3000.0f;
	const FPaintSplat A = Profile->BuildSplat(Hit, HeadOn, 2, 1.0f, 0.35f, 1234);
	const FPaintSplat B = Profile->BuildSplat(Hit, HeadOn, 2, 1.0f, 0.35f, 1234);
	const FPaintSplat C = Profile->BuildSplat(Hit, HeadOn, 2, 1.0f, 0.35f, 4321);

	TestEqual(TEXT("same seed, same center"), FVector(A.Location), FVector(B.Location));
	TestEqual(TEXT("same seed, same axis"), FVector(A.AxisU), FVector(B.AxisU));
	TestEqual(TEXT("same seed, same radius"), A.Radius, B.Radius);
	TestEqual(TEXT("head-on stretch"), A.Stretch, 1.0f, 1e-4f);
	TestEqual(TEXT("head-on center stays on the contact"), FVector(A.Location), Hit.ImpactPoint);
	TestTrue(TEXT("axis lies in the surface"), FMath::IsNearlyZero(FVector::DotProduct(FVector(A.AxisU), FVector::UpVector), 1e-3));
	TestNotEqual(TEXT("another seed spins the stamp"), FVector(A.AxisU), FVector(C.AxisU));

	// Grazing: the stamp stretches along the incident tangent and slides ahead of the contact.
	const FVector Grazing = (FVector::ForwardVector * 4.0 - FVector::UpVector).GetSafeNormal() * 3000.0;
	const FPaintSplat G = Profile->BuildSplat(Hit, Grazing, 2, 1.0f, 0.35f, 7);
	TestTrue(TEXT("grazing stretches"), G.Stretch > 1.5f);
	TestEqual(TEXT("stretched along the tangent"), FVector(G.AxisU), FVector::ForwardVector, 1e-3f);
	TestTrue(TEXT("center slides ahead"), G.Location.X > Hit.ImpactPoint.X);
	TestTrue(TEXT("impact sits behind the center"), G.ImpactU < 0.0f);
	return true;
}

#endif
