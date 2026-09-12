#include "Misc/AutomationTest.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintPlatformCoverage.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformCoverageScopeTest, "MintChoco.Paint.PlatformCoverage.MapScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPlatformCoverageScopeTest::RunTest(const FString& Parameters)
{
	const FString Map = TEXT("/Game/Maps/Lvl_Stage1_Test");
	TestTrue(TEXT("exact test map"), PaintPlatformCoverage::MatchesMap(Map, Map));
	TestTrue(TEXT("PIE client"), PaintPlatformCoverage::MatchesMap(TEXT("/Game/Maps/UEDPIE_2_Lvl_Stage1_Test"), Map));
	TestFalse(TEXT("original stage unaffected"), PaintPlatformCoverage::MatchesMap(TEXT("/Game/Maps/Lvl_Stage"), Map));
	TestFalse(TEXT("same short name elsewhere unaffected"), PaintPlatformCoverage::MatchesMap(TEXT("/Game/Sample/Lvl_Stage1_Test"), Map));
	TestFalse(TEXT("similarly named copy unaffected"), PaintPlatformCoverage::MatchesMap(Map + TEXT("_Copy"), Map));
	TestFalse(TEXT("empty setting disables"), PaintPlatformCoverage::MatchesMap(Map, TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformCoverageMaskTest, "MintChoco.Paint.PlatformCoverage.OwnershipMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPlatformCoverageMaskTest::RunTest(const FString& Parameters)
{
	const TArray<FVector3f> Vertices = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
	const TArray<uint32> Indices = {0, 1, 2, 0, 2, 3};
	FPaintCellGrid Grid;
	Grid.Build(FBox(FVector(0), FVector(100, 100, 1)), 25, 1, Vertices, {}, Indices);
	Grid.FilterSurfaceCells([](const FVector& Center, EPaintFaceDirection) { return Center.X < 50; });
	TestEqual(TEXT("only accessible half is denominator"), Grid.GetCoverage().TotalArea, 5000.0f, 0.1f);
	FPaintLocalStamp Stamp;
	Stamp.Center = FVector(50, 50, 0);
	Stamp.Normal = FVector::UpVector;
	Stamp.AxisU = FVector::ForwardVector;
	Stamp.AxisV = FVector::RightVector;
	Stamp.Radius = 1000;
	Stamp.Stretch = 1;
	Grid.Mark(Stamp, 0, 1);
	TestEqual(TEXT("painting all valid cells reaches 100 percent"), Grid.GetCoverage().GetFraction(0), 1.0f, 0.0001f);
	Grid.Mark(Stamp, 1, 1);
	TestEqual(TEXT("opponent replaces ownership"), Grid.GetCoverage().GetFraction(0), 0.0f, 0.0001f);
	TestEqual(TEXT("opponent cannot score excluded area"), Grid.GetCoverage().AreaByPaintId[1], 5000.0f, 0.1f);
	Grid.ClearPaint();
	TestEqual(TEXT("clear retains filtered denominator"), Grid.GetCoverage().TotalArea, 5000.0f, 0.1f);
	TestEqual(TEXT("clear resets valid area to bare"), Grid.GetCoverage().GetFraction(PaintIdNone), 1.0f, 0.0001f);
	Grid.FilterSurfaceCells([](const FVector&, EPaintFaceDirection) { return false; });
	TestEqual(TEXT("all excluded has zero denominator"), Grid.GetCoverage().TotalArea, 0.0f);
	TestEqual(TEXT("zero denominator has finite zero fraction"), Grid.GetCoverage().GetFraction(0), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformCoverageCollisionTest, "MintChoco.Paint.PlatformCoverage.Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPlatformCoverageCollisionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
		.RequiresHitProxies(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("world"), World) || !TestNotNull(TEXT("cube"), Cube))
	{
		if (World) World->DestroyWorld(false);
		return false;
	}
	const auto AddCube = [&](const FVector& Position, const FVector& Scale)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->SetActorLocation(Position);
		Actor->SetActorScale3D(Scale);
		return Mesh;
	};
	UStaticMeshComponent* Floor = AddCube(FVector(0, 0, -50), FVector(6, 6, 1));
	const auto Measure = [&]()
	{
		FPaintCellGrid Grid;
		const FBox Bounds = Floor->CalcBounds(FTransform::Identity).GetBox();
		Grid.BuildFromMesh(*Floor, 0, 25, Floor->GetComponentScale().GetAbs(), Bounds,
			PaintPlatformCoverage::ResolveDirections(*Floor, 0));
		PaintPlatformCoverage::Filter(*Floor, Grid);
		return Grid.GetCoverage().TotalArea;
	};
	const float Open = Measure();
	TestTrue(TEXT("open platform contributes area"), Open > 300000);
	UStaticMeshComponent* Obstacle = AddCube(FVector(0, 0, 100), FVector(2, 2, 2));
	const float Covered = Measure();
	TestTrue(TEXT("solid object removes underlying cells"), Covered < Open - 30000);
	Obstacle->GetOwner()->SetActorLocation(FVector(0, 0, 400));
	TestEqual(TEXT("high bridge keeps lower floor"), Measure(), Open, 1.0f);
	Obstacle->GetOwner()->SetActorLocation(FVector(0, 0, 180));
	TestTrue(TEXT("low ceiling excludes space without standing room"), Measure() < Open);
	Obstacle->GetOwner()->Destroy();
	Floor->GetOwner()->SetActorRotation(FRotator(25, 0, 0));
	TestTrue(TEXT("walkable rotated slope retains area"), Measure() > 0);
	Floor->GetOwner()->SetActorRotation(FRotator(90, 0, 0));
	TestFalse(TEXT("vertical face not walkable"), PaintPlatformCoverage::IsWalkableNormal(*Floor, FVector::ForwardVector));
	TestEqual(TEXT("capsule on flat surface"), PaintPlatformCoverage::CapsuleCenterHeight(34, 88, 1), 90.0f);
	TestTrue(TEXT("slope clearance accounts for capsule radius"), PaintPlatformCoverage::CapsuleCenterHeight(34, 88, 0.8f) > 90);
	UStaticMesh* Ramp = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Maps/Stage1Test/SM_PaintableRamp.SM_PaintableRamp"));
	if (TestNotNull(TEXT("test map's CPU readable ramp"), Ramp))
	{
		Floor->SetStaticMesh(Ramp);
		Floor->GetOwner()->SetActorRotation(FRotator::ZeroRotator);
		Floor->GetOwner()->SetActorScale3D(FVector(6, 12, 3.5));
		TestTrue(TEXT("non-uniform wedge with a tied local normal retains walkable slope"), Measure() > 500000);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
