#include "Misc/AutomationTest.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraDataInterfaceExport.h"

#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintSplash.h"
#include "Paint/PaintSplashProfile.h"
#include "Paint/PaintSplashSubsystem.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#define SPLASH_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
	UPaintSplashProfile* MakeProfile()
	{
		UPaintSplashProfile* const Profile = NewObject<UPaintSplashProfile>();
		Profile->DropletBrush = NewObject<UPaintBrushProfile>();
		// Any material will do for a brush nothing renders; the engine's default exists in every runner.
		Profile->DropletBrush->BrushMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
		return Profile;
	}

	/** A ball coming down onto a floor at the origin, fast and at a slant: the everyday contact. */
	PaintSplash::FSpawnInput FloorHit(int32 Seed)
	{
		PaintSplash::FSpawnInput Input;
		Input.ImpactPoint = FVector::ZeroVector;
		Input.ImpactNormal = FVector::UpVector;
		Input.IncidentVelocity = FVector(1500.0, 0.0, -2500.0);
		Input.BallRadius = 6.0f;
		Input.Seed = Seed;
		return Input;
	}

	/**
	 * A cube floor with its top face at Z = 0 that keeps paint on its up direction. Null when the
	 * runner cannot load the project's CPU-readable cube (the game binary only sees cooked packages).
	 */
	UPaintableComponent* SpawnPaintableFloor(UWorld& World)
	{
		// The score grid is built from LOD 0 on the CPU, which the engine's basic shapes do not allow outside the editor.
		UStaticMesh* const Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Paint/SM_PaintableCube.SM_PaintableCube"));
		if (!Cube)
		{
			return nullptr;
		}
		// Deferred, so the mesh and its transform are in place before the static component
		// registers; a registered static mesh in a game world refuses to move or change mesh.
		const FVector Scale(6.0, 6.0, 1.0);
		const FTransform Transform(FRotator::ZeroRotator, FVector(0.0, 0.0, -Cube->GetBoundingBox().Max.Z * Scale.Z), Scale);
		AStaticMeshActor* const Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		if (!Actor)
		{
			return nullptr;
		}
		UStaticMeshComponent* const Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->FinishSpawning(Transform);

		UPaintableComponent* const Paintable = NewObject<UPaintableComponent>(Actor, NAME_None, RF_Transient);
		Actor->AddInstanceComponent(Paintable);
		Paintable->RegisterComponent();
		// The test world never brings its actors to play by itself; the surface builds its grid in BeginPlay.
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Paintable;
	}

	const TCHAR* const FloorUnavailable = TEXT("skipped: this runner cannot load /Game/LevelPrototyping/Paint/SM_PaintableCube (only the editor sees uncooked content).");
}

/** The droplets are a pure function of the contact and the seed, and they all leave the surface. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaintSplashDeterminismTest, "MintChoco.Paint.Splash.Determinism", SPLASH_TEST_FLAGS)

bool FPaintSplashDeterminismTest::RunTest(const FString& Parameters)
{
	UPaintSplashProfile* const Profile = MakeProfile();

	TArray<PaintSplash::FDroplet> A;
	TArray<PaintSplash::FDroplet> B;
	PaintSplash::GenerateDroplets(*Profile, FloorHit(7), A);
	PaintSplash::GenerateDroplets(*Profile, FloorHit(7), B);
	TestEqual(TEXT("one jet plus the satellites"), A.Num(), 1 + Profile->SatelliteCount);
	if (A.Num() != B.Num())
	{
		return false;
	}
	bool bSame = true;
	bool bLeaving = true;
	bool bWithinSpeed = true;
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		bSame &= A[Index].Position.Equals(B[Index].Position) && A[Index].Velocity.Equals(B[Index].Velocity)
			&& FMath::IsNearlyEqual(A[Index].Radius, B[Index].Radius);
		bLeaving &= FVector::DotProduct(A[Index].Velocity, FVector::UpVector) > 0.0;
		bWithinSpeed &= A[Index].Velocity.Size() <= Profile->MaxDropletSpeed + 1e-3;
	}
	TestTrue(TEXT("same seed, same droplets"), bSame);
	TestTrue(TEXT("droplet 0 is the jet"), A[0].bJet);
	TestTrue(TEXT("every droplet leaves the surface"), bLeaving);
	TestTrue(TEXT("no droplet exceeds MaxDropletSpeed"), bWithinSpeed);

	TArray<PaintSplash::FDroplet> C;
	PaintSplash::GenerateDroplets(*Profile, FloorHit(8), C);
	TestFalse(TEXT("another seed throws differently"), C.Num() > 1 && C[1].Velocity.Equals(A[1].Velocity));

	// A wall hit at a slant: the frame follows the surface, not the world.
	PaintSplash::FSpawnInput Wall = FloorHit(3);
	Wall.ImpactNormal = FVector::ForwardVector;
	Wall.IncidentVelocity = FVector(-2000.0, 300.0, -400.0);
	Profile->SatelliteTangentBias = 1.0f;
	TArray<PaintSplash::FDroplet> W;
	PaintSplash::GenerateDroplets(*Profile, Wall, W);
	const FVector Forward = FVector(0.0, 300.0, -400.0).GetSafeNormal();
	bool bOffWall = true;
	bool bAhead = true;
	for (int32 Index = 0; Index < W.Num(); ++Index)
	{
		bOffWall &= FVector::DotProduct(W[Index].Velocity, Wall.ImpactNormal) > 0.0;
		if (Index > 0)
		{
			bAhead &= FVector::DotProduct(W[Index].Velocity, Forward) >= -1e-3;
		}
	}
	TestEqual(TEXT("the wall hit splashes too"), W.Num(), 1 + Profile->SatelliteCount);
	TestTrue(TEXT("every droplet leaves the wall"), bOffWall);
	TestTrue(TEXT("with full bias every satellite goes forward"), bAhead);

	// A dying lob only splats.
	PaintSplash::FSpawnInput Slow = FloorHit(1);
	Slow.IncidentVelocity = FVector(100.0, 0.0, -300.0);
	TArray<PaintSplash::FDroplet> S;
	PaintSplash::GenerateDroplets(*Profile, Slow, S);
	TestEqual(TEXT("below MinNormalSpeed there is no splash"), S.Num(), 0);
	return true;
}

/** The droplets never hold more paint than the ball brought. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaintSplashVolumeCapTest, "MintChoco.Paint.Splash.VolumeCap", SPLASH_TEST_FLAGS)

bool FPaintSplashVolumeCapTest::RunTest(const FString& Parameters)
{
	float Radii[] = {6.0f, 6.0f, 6.0f, 6.0f};
	const float Scale = PaintSplash::CapVolume(Radii, 6.0f, 0.5f);
	TestEqual(TEXT("four full-size droplets shrink to half the volume together"), Scale, 0.5f, 1e-4f);
	TestEqual(TEXT("each radius follows the scale"), Radii[2], 3.0f, 1e-4f);

	float Small[] = {1.0f, 1.0f};
	TestEqual(TEXT("nothing to cap returns 1"), PaintSplash::CapVolume(Small, 6.0f, 0.5f), 1.0f);

	UPaintSplashProfile* const Profile = MakeProfile();
	Profile->JetRadiusScale = 2.0f;
	Profile->SatelliteRadiusScale = 2.0f;
	Profile->VolumeFraction = 0.5f;
	TArray<PaintSplash::FDroplet> Droplets;
	PaintSplash::GenerateDroplets(*Profile, FloorHit(5), Droplets);
	double Total = 0.0;
	for (const PaintSplash::FDroplet& Droplet : Droplets)
	{
		Total += FMath::Cube(static_cast<double>(Droplet.Radius));
	}
	TestTrue(TEXT("generated droplets respect VolumeFraction"), Total <= FMath::Cube(6.0) * 0.5 * (1.0 + 1e-3));
	return true;
}

/** The score's guess at where the droplets come down: on the floor, near the contact, and never off a wall. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaintSplashPhantomLandingsTest, "MintChoco.Paint.Splash.PhantomLandings", SPLASH_TEST_FLAGS)

bool FPaintSplashPhantomLandingsTest::RunTest(const FString& Parameters)
{
	UPaintSplashProfile* const Profile = MakeProfile();
	constexpr float GravityZ = -980.0f;

	TArray<PaintSplash::FPhantomLanding> Landings;
	PaintSplash::PhantomLandings(*Profile, FloorHit(7), GravityZ, Landings);
	TestEqual(TEXT("every droplet of a floor hit comes back down"), Landings.Num(), 1 + Profile->SatelliteCount);
	bool bOnPlane = true;
	bool bInReach = true;
	bool bMoving = true;
	for (const PaintSplash::FPhantomLanding& Landing : Landings)
	{
		bOnPlane &= FMath::Abs(Landing.Point.Z) < 0.5;
		bInReach &= Landing.Point.Size() <= Profile->MaxTravel;
		bMoving &= Landing.Speed > 0.0f;
	}
	TestTrue(TEXT("landings lie on the contact plane"), bOnPlane);
	TestTrue(TEXT("landings stay within MaxTravel"), bInReach);
	TestTrue(TEXT("landings arrive with speed"), bMoving);
	if (Landings.Num() > 0)
	{
		// The jet goes up the most, so it lands last and, leaning forward, ahead of the contact.
		TestTrue(TEXT("the jet lands ahead of the contact"), Landings[0].Point.X > 0.0);
	}

	// Same seed, same landings: this is what keeps every machine's score grid identical.
	TArray<PaintSplash::FPhantomLanding> Again;
	PaintSplash::PhantomLandings(*Profile, FloorHit(7), GravityZ, Again);
	TestTrue(TEXT("landings are deterministic"),
		Again.Num() == Landings.Num() && Again.Num() > 0 && Again[0].Point.Equals(Landings[0].Point));

	PaintSplash::FSpawnInput Wall = FloorHit(2);
	Wall.ImpactNormal = FVector::ForwardVector;
	Wall.IncidentVelocity = FVector(-2000.0, 0.0, -400.0);
	PaintSplash::PhantomLandings(*Profile, Wall, GravityZ, Landings);
	TestEqual(TEXT("a wall hit lands nothing on the wall"), Landings.Num(), 0);

	PaintSplash::FSpawnInput Ceiling = FloorHit(2);
	Ceiling.ImpactNormal = FVector::DownVector;
	Ceiling.IncidentVelocity = FVector(500.0, 0.0, 2500.0);
	PaintSplash::PhantomLandings(*Profile, Ceiling, GravityZ, Landings);
	TestEqual(TEXT("a ceiling hit lands nothing on the ceiling"), Landings.Num(), 0);

	// The look helpers the blob shader mirrors.
	TestEqual(TEXT("cohesion starts at CohesionRadius"), PaintSplash::Cohesion(*Profile, 0.0f), Profile->CohesionRadius);
	TestEqual(TEXT("cohesion is gone after CohesionDecay"), PaintSplash::Cohesion(*Profile, Profile->CohesionDecay * 2.0f), 0.0f);
	float Radius = 0.0f;
	float Tube = 0.0f;
	float Fade = 0.0f;
	PaintSplash::CrownAt(*Profile, 6.0f, 0.0f, Radius, Tube, Fade);
	TestEqual(TEXT("the crown starts at half the ball"), Radius, 3.0f, 1e-4f);
	TestEqual(TEXT("the crown starts opaque"), Fade, 1.0f);
	PaintSplash::CrownAt(*Profile, 6.0f, Profile->CrownLifetime, Radius, Tube, Fade);
	TestEqual(TEXT("the crown ends at CrownRadiusScale"), Radius, Profile->CrownRadiusScale * 6.0f, 1e-3f);
	TestEqual(TEXT("the crown ends thin"), Tube, 0.0f);
	TestEqual(TEXT("the crown ends faded"), Fade, 0.0f);
	return true;
}

/** A splat that carries a splash profile scores its phantom landings too, and a score-only splat needs no atlas. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaintSplashScoreTest, "MintChoco.Paint.Splash.ScoresPhantomLandings", SPLASH_TEST_FLAGS)

bool FPaintSplashScoreTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	UPaintableComponent* const Floor = SpawnPaintableFloor(*World);
	if (!Floor)
	{
		AddWarning(FloorUnavailable);
		return true;
	}
	UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>();
	if (!TestNotNull(TEXT("paint subsystem"), Paint) || !TestTrue(TEXT("the floor has score cells"), Floor->GetCoverage().TotalArea > 0.0f))
	{
		return false;
	}

	// Score only: the grid is built at BeginPlay, the atlas is not, and this test never ticks.
	FPaintSplat Splat;
	Splat.Location = FVector::ZeroVector;
	Splat.Normal = FVector::UpVector;
	Splat.AxisU = FVector::ForwardVector;
	Splat.Radius = 25.0f;
	Splat.PaintId = 0;
	Splat.Seed = 7;
	Splat.bScoreOnly = true;
	Paint->ApplySplat(Splat);
	const float Plain = Floor->GetCoverage().AreaByPaintId[0];
	TestTrue(TEXT("a score-only splat marks cells without an atlas"), Plain > 0.0f);

	Floor->ClearPaint();
	// A phantom claims a cell only when the cell's centre lies inside its stamp. A mark as wide as a
	// cell reaches a centre from anywhere on the face, so every landing counts wherever the seed
	// scatters it; the brush numbers themselves are tuning, not what this test guards.
	UPaintSplashProfile* const Profile = MakeProfile();
	const float CellSize = Floor->GetScoreCellSize();
	Profile->DropletBrush->BaseRadius = CellSize;
	Profile->DropletBrush->RadiusPerSpeed = 0.0f;
	Profile->DropletBrush->MaxRadius = CellSize;
	Profile->DropletSplatVolume = 1.0f;
	Splat.Splash = Profile;
	Splat.IncidentDir = FVector(1500.0, 0.0, -2500.0).GetSafeNormal();
	Splat.IncidentSpeed = 2915;
	Splat.BallRadius = 6;
	Paint->ApplySplat(Splat);
	const float WithSplash = Floor->GetCoverage().AreaByPaintId[0];
	TestTrue(TEXT("the phantom landings add cells beyond the splat"), WithSplash > Plain);

	// A phantom expands nothing itself: applying the same contact twice only repaints the same cells.
	Paint->ApplySplat(Splat);
	TestEqual(TEXT("re-applying the contact changes no more cells"), Floor->GetCoverage().AreaByPaintId[0], WithSplash, 1e-3f);

	// A droplet's mark is the picture alone: the grid does not move.
	FPaintSplat Mark = Splat;
	Mark.Splash = nullptr;
	Mark.bScoreOnly = false;
	Mark.bDrawOnly = true;
	Mark.PaintId = 1;
	Mark.Location = FVector(100.0, 0.0, 0.0);
	Paint->ApplySplat(Mark);
	TestEqual(TEXT("a draw-only splat claims no cell"), Floor->GetCoverage().AreaByPaintId[1], 0.0f);
	return true;
}

/** Landing handlers are pooled: one per splash while it flies, free again when the effect is done. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaintSplashHandlerPoolTest, "MintChoco.Paint.Splash.HandlerPool", SPLASH_TEST_FLAGS)

bool FPaintSplashHandlerPoolTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	UPaintSplashSubsystem* const Splash = World->GetSubsystem<UPaintSplashSubsystem>();
	if (!TestNotNull(TEXT("splash subsystem"), Splash))
	{
		return false;
	}

	FPaintSplashRequest Request;
	Request.Profile = MakeProfile();
	Request.ImpactPoint = FVector::ZeroVector;
	Request.ImpactNormal = FVector::UpVector;
	Request.IncidentVelocity = FVector(1500.0, 0.0, -2500.0);
	Request.Seed = 1;
	if (!Request.Profile->DropletBrush->BrushMaterial)
	{
		AddWarning(TEXT("skipped: the engine's default material did not load in this runner."));
		return true;
	}

	UPaintSplashLandingHandler* const First = Splash->BeginSplash(Request);
	if (!TestNotNull(TEXT("a floor contact books a handler"), First))
	{
		return false;
	}
	TestTrue(TEXT("the handler is armed"), First->IsArmed());
	UPaintSplashLandingHandler* const Second = Splash->BeginSplash(Request);
	TestTrue(TEXT("a second contact in flight gets its own handler"), Second && Second != First);
	TestEqual(TEXT("two handlers armed"), Splash->GetArmedHandlerCount(), 2);

	First->Release();
	TestEqual(TEXT("a released handler is reused before a new one is made"), Splash->BeginSplash(Request), First);

	Request.bLeavesMarks = false;
	TestNull(TEXT("a contact that leaves no marks books nothing"), Splash->BeginSplash(Request));

	// A landing on the floor is a draw-only splat: it must not touch the score, and it must not need a floor to be safe.
	UPaintableComponent* const Floor = SpawnPaintableFloor(*World);
	TArray<FBasicParticleData> Data;
	FBasicParticleData& Landing = Data.AddDefaulted_GetRef();
	Landing.Position = FVector(50.0, 0.0, 0.0);
	Landing.Size = 300.0f;
	Landing.Velocity = FVector::UpVector;
	// Straight into the native implementation: the interface's Execute_ thunk is not exported by the Niagara module.
	First->ReceiveParticleData_Implementation(Data, nullptr, FVector::ZeroVector);
	TestEqual(TEXT("the handler counted the landing"), First->GetLandingCount(), 1);
	if (Floor)
	{
		TestEqual(TEXT("a droplet's mark claims no cell"), Floor->GetCoverage().AreaByPaintId[0], 0.0f);
	}
	return true;
}

#undef SPLASH_TEST_FLAGS

#endif
