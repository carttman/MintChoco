#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectIterator.h"

#include "Game/NetAssetRetainerSubsystem.h"
#include "Game/Unit.h"
#include "Game/UnitDataAsset.h"
#include "Items/ItemPickup.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "Paint/PaintBrushProfile.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

/**
 * 네트워크로 참조되는 자산이 빠짐없이 붙들리는지 지킨다. 하나라도 빠지면 그 자산을 GC한
 * 클라이언트에서만 조용히 사라진다(페인트가 안 칠해지고 아이템이 안 보인다). 이름이 아니라
 * 클래스로 세므로, 자산이 이름을 바꿔도 검사에서 빠지지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNetAssetRetainerTest,
	"MintChoco.Net.AssetRetainer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNetAssetRetainerTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	// 커맨드릿 실행은 첫 스캔이 끝나지 않았을 수 있다.
	Registry.ScanPathsSynchronous({TEXT("/Game")}, /*bForceRescan=*/false);

	// 서브시스템은 GameInstance 안에서만 만들어질 수 있다(ClassWithin).
	UGameInstance* const GameInstance = NewObject<UGameInstance>(GEngine);
	UNetAssetRetainerSubsystem* const Retainer = NewObject<UNetAssetRetainerSubsystem>(GameInstance);
	const int32 Count = Retainer->Retain();
	TestTrue(TEXT("Retain finds something"), Count > 0);

	// 모든 대상 클래스의 모든 자산이 붙들린다.
	for (const UClass* const Class : UNetAssetRetainerSubsystem::GetRetainedAssetClasses())
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Found;
		Registry.GetAssets(Filter, Found);

		for (const FAssetData& Asset : Found)
		{
			UObject* const Loaded = Asset.GetAsset();
			TestNotNull(*FString::Printf(TEXT("%s loads"), *Asset.GetObjectPathString()), Loaded);
			TestTrue(*FString::Printf(TEXT("%s is retained"), *Asset.GetObjectPathString()), Retainer->IsRetained(Loaded));
		}
	}

	// 필터가 조용히 아무것도 못 찾는 회귀를 막는다: 경기에 꼭 있어야 하는 세 종류는 반드시 하나 이상.
	const auto CountRetainedOfClass = [Retainer](const UClass* Class)
	{
		int32 Num = 0;
		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Found;
		IAssetRegistry::GetChecked().GetAssets(Filter, Found);
		for (const FAssetData& Asset : Found)
		{
			if (Retainer->IsRetained(Asset.GetAsset()))
			{
				++Num;
			}
		}
		return Num;
	};
	TestTrue(TEXT("At least one item profile is retained"), CountRetainedOfClass(UItemProfile::StaticClass()) > 0);
	TestTrue(TEXT("At least one brush profile is retained"), CountRetainedOfClass(UPaintBrushProfile::StaticClass()) > 0);
	TestTrue(TEXT("At least one unit data asset is retained"), CountRetainedOfClass(UUnitDataAsset::StaticClass()) > 0);

	// 설정의 스폰 후보와 픽업 클래스.
	const UItemSettings& ItemSettings = UItemSettings::Get();
	TArray<UItemProfile*> Items;
	ItemSettings.LoadItems(Items);
	TestTrue(TEXT("Item settings list is not empty"), Items.Num() > 0);
	for (const UItemProfile* const Item : Items)
	{
		TestTrue(*FString::Printf(TEXT("Settings item %s is retained"), *GetNameSafe(Item)), Retainer->IsRetained(Item));
	}
	UClass* const PickupClass = ItemSettings.LoadPickupClass();
	TestNotNull(TEXT("Pickup class is set"), PickupClass);
	TestTrue(TEXT("Pickup class is retained"), Retainer->IsRetained(PickupClass));

	// 서버가 스폰하는 액터의 블루프린트 클래스: 유닛과 픽업이 하나씩은 있어야 한다.
	int32 UnitClasses = 0;
	int32 PickupClasses = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!Retainer->IsRetained(*It) || It->HasAnyClassFlags(CLASS_Native))
		{
			continue;
		}
		UnitClasses += It->IsChildOf(AUnit::StaticClass()) ? 1 : 0;
		PickupClasses += It->IsChildOf(AItemPickup::StaticClass()) ? 1 : 0;
	}
	TestTrue(TEXT("A unit Blueprint class is retained"), UnitClasses > 0);
	TestTrue(TEXT("A pickup Blueprint class is retained"), PickupClasses > 0);

	// 다시 불러도 늘어나지 않는다.
	TestEqual(TEXT("Retain is idempotent"), Retainer->Retain(), Count);

	return true;
}

#endif
