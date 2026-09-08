#include "Misc/AutomationTest.h"

#include "Game/GameGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 스폰 지점 선택: 차 있는 지점은 절대 고르지 않고, 전부 차 있으면 없다고 답하며, 시드가 같으면 답도 같다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemSpawnPickFreePointTest,
	"MintChoco.Items.Spawn.PickFreePoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemSpawnPickFreePointTest::RunTest(const FString& Parameters)
{
	const FRandomStream Random(1234);

	TestEqual(TEXT("no points"), AGameGameMode::PickFreeSpawnIndex({}, Random), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("all occupied"), AGameGameMode::PickFreeSpawnIndex({false, false, false}, Random), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the only free one"), AGameGameMode::PickFreeSpawnIndex({false, true, false}, Random), 1);

	// 여러 번 뽑아도 차 있는 지점(0, 3)은 나오지 않고, 빈 지점은 모두 언젠가 나온다.
	const TArray<bool> bFree = {false, true, true, false, true};
	TSet<int32> Seen;
	for (int32 Draw = 0; Draw < 200; ++Draw)
	{
		const int32 Index = AGameGameMode::PickFreeSpawnIndex(bFree, Random);
		TestTrue(TEXT("picked index is in range"), bFree.IsValidIndex(Index));
		if (bFree.IsValidIndex(Index))
		{
			TestTrue(TEXT("picked index is free"), bFree[Index]);
			Seen.Add(Index);
		}
	}
	TestEqual(TEXT("every free point gets picked eventually"), Seen.Num(), 3);

	const FRandomStream A(42);
	const FRandomStream B(42);
	for (int32 Draw = 0; Draw < 20; ++Draw)
	{
		TestEqual(TEXT("same seed, same pick"), AGameGameMode::PickFreeSpawnIndex(bFree, A), AGameGameMode::PickFreeSpawnIndex(bFree, B));
	}

	return true;
}

#endif
