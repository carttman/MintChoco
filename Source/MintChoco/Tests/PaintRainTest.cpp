#include "Misc/AutomationTest.h"

#include "Items/PaintRain.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 폭격 계획: 행은 경계 안의 마지막 지점까지, 열은 진행 방향의 좌우로 펼쳐진다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRainPlanTest,
	"MintChoco.Items.Rain.Plan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRainPlanTest::RunTest(const FString& Parameters)
{
	const FBox2D Bounds(FVector2D(0.0f, 0.0f), FVector2D(1000.0f, 1000.0f));

	// 안에서 +X로: 250, 400, ..., 1000까지 6행.
	TestEqual(TEXT("rows to the edge"), FPaintRainPlan::CountRows(FVector2D(100.0f, 100.0f), FVector2D(1.0f, 0.0f), Bounds, 150.0f, 200), 6);

	// 밖에서 시작해도 반대편 끝까지: -500 + 150k <= 1000 → k = 10.
	TestEqual(TEXT("from outside to the far edge"), FPaintRainPlan::CountRows(FVector2D(-500.0f, 100.0f), FVector2D(1.0f, 0.0f), Bounds, 150.0f, 200), 10);

	// 맵을 등지면 한 행도 없다.
	TestEqual(TEXT("aiming away yields nothing"), FPaintRainPlan::CountRows(FVector2D(100.0f, 100.0f), FVector2D(-1.0f, 0.0f), Bounds, 150.0f, 200), 0);

	// 상한.
	TestEqual(TEXT("capped by MaxRows"), FPaintRainPlan::CountRows(FVector2D(0.0f, 500.0f), FVector2D(1.0f, 0.0f), Bounds, 10.0f, 5), 5);

	// 방향이 정규화되지 않아도 같다.
	TestEqual(TEXT("direction is normalized"), FPaintRainPlan::CountRows(FVector2D(100.0f, 100.0f), FVector2D(5.0f, 0.0f), Bounds, 150.0f, 200), 6);

	// 열: 진행 +X, 오른쪽은 +Y. 세 열은 -100, 0, +100.
	FPaintRainParams Params;
	Params.Origin = FVector(0.0f, 0.0f, 0.0f);
	Params.Direction = FVector2D(1.0f, 0.0f);
	Params.RowSpacing = 150.0f;
	Params.Columns = 3;
	Params.ColumnSpacing = 100.0f;
	Params.DropZ = 800.0f;
	const FVector Left = FPaintRainPlan::RowPoint(Params, 2, 0);
	const FVector Middle = FPaintRainPlan::RowPoint(Params, 2, 1);
	const FVector Right = FPaintRainPlan::RowPoint(Params, 2, 2);
	TestTrue(TEXT("middle column is on the line"), Middle.Equals(FVector(300.0f, 0.0f, 800.0f)));
	TestTrue(TEXT("left column"), Left.Equals(FVector(300.0f, -100.0f, 800.0f)));
	TestTrue(TEXT("right column"), Right.Equals(FVector(300.0f, 100.0f, 800.0f)));

	Params.Columns = 1;
	TestTrue(TEXT("a single column sits on the line"), FPaintRainPlan::RowPoint(Params, 1, 0).Equals(FVector(150.0f, 0.0f, 800.0f)));

	return true;
}

#endif
