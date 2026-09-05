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
