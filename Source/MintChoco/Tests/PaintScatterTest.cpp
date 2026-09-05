#include "Misc/AutomationTest.h"

#include "Weapons/PaintScatterProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintScatterDeterministicTest,
	"MintChoco.Paint.Weapons.ScatterDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintScatterDeterministicTest::RunTest(const FString& Parameters)
{
	UPaintScatterProfile* const Scatter = NewObject<UPaintScatterProfile>();
	Scatter->PelletsPerShot = 16;
	Scatter->SpreadHalfAngleDeg = 5.0f;

	const FVector Forward = FVector::ForwardVector;
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(5.0f)) - 1e-4f;

	TArray<FVector> First;
	TArray<FVector> Same;
	TArray<FVector> Other;
	Scatter->ComputePelletDirections(Forward, 1234, First);
	Scatter->ComputePelletDirections(Forward, 1234, Same);
	Scatter->ComputePelletDirections(Forward, 4321, Other);

	TestEqual(TEXT("pellet count"), First.Num(), 16);
	TestEqual(TEXT("same seed, same count"), Same.Num(), First.Num());

	bool bAnyDifferent = false;
	for (int32 Pellet = 0; Pellet < First.Num(); ++Pellet)
	{
		TestTrue(TEXT("same seed, same direction"), First[Pellet].Equals(Same[Pellet], 1e-6));
		TestTrue(TEXT("unit length"), FMath::IsNearlyEqual(First[Pellet].Size(), 1.0, 1e-4));
		TestTrue(TEXT("inside the cone"), FVector::DotProduct(First[Pellet], Forward) >= MinCos);
		bAnyDifferent |= !First[Pellet].Equals(Other[Pellet], 1e-6);
	}
	TestTrue(TEXT("different seed, different pattern"), bAnyDifferent);

	Scatter->SpreadHalfAngleDeg = 0.0f;
	TArray<FVector> Straight;
	Scatter->ComputePelletDirections(Forward, 1234, Straight);
	for (const FVector& Direction : Straight)
	{
		TestTrue(TEXT("zero spread fires straight"), Direction.Equals(Forward, 0.0));
	}

	return true;
}

#endif
