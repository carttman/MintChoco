#include "Misc/AutomationTest.h"

#include "Weapons/PaintBurst.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 파열 방향은 시드에 결정적이고, 고도 범위 안의 단위 벡터다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintBurstDirectionsTest,
	"MintChoco.Weapons.Burst.Directions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintBurstDirectionsTest::RunTest(const FString& Parameters)
{
	TArray<FVector> A;
	TArray<FVector> B;
	PaintBurst::ComputeDirections(7, 24, 10.0f, 70.0f, A);
	PaintBurst::ComputeDirections(7, 24, 10.0f, 70.0f, B);
	TestEqual(TEXT("24 directions"), A.Num(), 24);

	bool bSame = true;
	bool bInRange = true;
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		bSame &= A[Index].Equals(B[Index]);
		const float Pitch = FMath::RadiansToDegrees(FMath::Asin(A[Index].Z));
		bInRange &= A[Index].IsNormalized() && Pitch >= 10.0f - 0.01f && Pitch <= 70.0f + 0.01f;
	}
	TestTrue(TEXT("same seed, same directions"), bSame);
	TestTrue(TEXT("every direction is unit length within the pitch range"), bInRange);

	TArray<FVector> C;
	PaintBurst::ComputeDirections(8, 24, 10.0f, 70.0f, C);
	TestFalse(TEXT("another seed differs"), A[0].Equals(C[0]));

	// 방위각이 고르게 돈다: 첫 방향과 열세 번째 방향은 대략 반대편이다.
	FVector First = A[0];
	FVector Opposite = A[12];
	First.Z = 0.0f;
	Opposite.Z = 0.0f;
	TestTrue(TEXT("azimuths are spread around the circle"), FVector::DotProduct(First.GetSafeNormal(), Opposite.GetSafeNormal()) < -0.5f);

	// 뒤집힌 범위도 받아들인다.
	TArray<FVector> D;
	PaintBurst::ComputeDirections(1, 4, 70.0f, 10.0f, D);
	TestEqual(TEXT("swapped pitch range still yields directions"), D.Num(), 4);

	// 반경 → 속도: 문서에 적힌 표(중력 0.5에서 3 m는 약 385)와 맞는다.
	TestEqual(TEXT("3 m at half gravity"), PaintBurst::SpeedForRange(300.0f, 0.5f), 383.4f, 1.0f);
	TestEqual(TEXT("4 m at half gravity"), PaintBurst::SpeedForRange(400.0f, 0.5f), 442.7f, 1.0f);
	TestTrue(TEXT("a bigger radius needs a faster ball"),
		PaintBurst::SpeedForRange(600.0f, 0.5f) > PaintBurst::SpeedForRange(300.0f, 0.5f));
	TestEqual(TEXT("zero range needs no speed"), PaintBurst::SpeedForRange(0.0f, 0.5f), 0.0f);

	return true;
}

#endif
