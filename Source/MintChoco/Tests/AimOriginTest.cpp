#include "Misc/AutomationTest.h"

#include "Weapons/PaintAimMath.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 발사 원점은 시선 위, 폰의 깊이. 애니메이션이 아니라 카메라가 정한다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAimFireOriginTest,
	"MintChoco.Weapons.Aim.FireOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAimFireOriginTest::RunTest(const FString& Parameters)
{
	// 3인칭 카메라: 폰 뒤 350, 위 60, 오른쪽 60에서 앞을 본다.
	const FVector Pawn(0.0, 0.0, 90.0);
	const FVector View(-350.0, 60.0, 150.0);
	const FVector Forward = FVector::ForwardVector;

	const FVector Origin = PaintAim::FireOrigin(View, Forward, Pawn, 30.0f);
	TestEqual(TEXT("depth is the pawn's plus the margin"), Origin.X, 30.0, 1e-3);
	TestEqual(TEXT("stays on the sight line (Y)"), Origin.Y, 60.0, 1e-3);
	TestEqual(TEXT("stays on the sight line (Z)"), Origin.Z, 150.0, 1e-3);
	TestTrue(TEXT("the origin lies on the view ray"), FVector::CrossProduct(Origin - View, Forward).IsNearlyZero(1e-3));

	// 시선이 기울어도 레이 위, 폰의 깊이다.
	const FVector Tilted = FVector(1.0, 0.3, -0.5).GetSafeNormal();
	const FVector TiltedOrigin = PaintAim::FireOrigin(View, Tilted, Pawn, 0.0f);
	TestTrue(TEXT("a tilted view keeps the origin on its ray"), FVector::CrossProduct(TiltedOrigin - View, Tilted).IsNearlyZero(1e-3));
	TestEqual(TEXT("a tilted view keeps the pawn's depth"),
		FVector::DotProduct(TiltedOrigin - View, Tilted), FVector::DotProduct(Pawn - View, Tilted), 1e-3);

	// 폰이 카메라 뒤에 있으면 마진만 남는다.
	const FVector Behind = PaintAim::FireOrigin(FVector(100.0, 0.0, 0.0), Forward, Pawn, 30.0f);
	TestEqual(TEXT("a pawn behind the camera leaves only the margin"), Behind.X, 130.0, 1e-3);
	return true;
}

/** 총구에서 궤적으로 합류하는 곡선: 0에서 시작해 MergeSeconds에 1, 되돌아가지 않는다. 0초면 즉시. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintAimMergeAlphaTest,
	"MintChoco.Weapons.Aim.MergeAlpha",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintAimMergeAlphaTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("starts at the muzzle"), PaintAim::MergeAlpha(0.0f, 0.12f), 0.0f, 1e-4f);
	TestEqual(TEXT("ends on the path"), PaintAim::MergeAlpha(0.12f, 0.12f), 1.0f, 1e-4f);
	TestEqual(TEXT("stays on the path afterwards"), PaintAim::MergeAlpha(1.0f, 0.12f), 1.0f, 1e-4f);
	TestEqual(TEXT("halfway is halfway"), PaintAim::MergeAlpha(0.06f, 0.12f), 0.5f, 1e-4f);
	TestEqual(TEXT("no merge time merges at once"), PaintAim::MergeAlpha(0.0f, 0.0f), 1.0f, 1e-4f);

	float Previous = 0.0f;
	bool bMonotonic = true;
	for (int32 Step = 1; Step <= 12; ++Step)
	{
		const float Alpha = PaintAim::MergeAlpha(0.01f * Step, 0.12f);
		bMonotonic &= Alpha >= Previous;
		Previous = Alpha;
	}
	TestTrue(TEXT("never slides back towards the muzzle"), bMonotonic);
	return true;
}

#endif
