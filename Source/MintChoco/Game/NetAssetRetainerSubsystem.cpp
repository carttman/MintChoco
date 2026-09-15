#include "Game/NetAssetRetainerSubsystem.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"

#include "Audio/SoundBank.h"
#include "Game/Unit.h"
#include "Game/UnitDataAsset.h"
#include "Game/UnitInputConfig.h"
#include "Items/ItemPickup.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "MintChoco.h"
#include "Paint/PaintBrushProfile.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintWeaponProfile.h"
#include "Weapons/PaintballProfile.h"

namespace
{
	const FName GameContentRoot(TEXT("/Game"));

	/** /Game 아래에서 Class와 그 하위 클래스의 자산을 전부 찾는다. */
	void FindAssetsOfClass(IAssetRegistry& Registry, const FTopLevelAssetPath& ClassPath, TArray<FAssetData>& OutFound)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(ClassPath);
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(GameContentRoot);
		Filter.bRecursivePaths = true;
		Registry.GetAssets(Filter, OutFound);
	}

	/** 블루프린트 태그 값("/Script/CoreUObject.Class'/Game/X.Y_C'")을 오브젝트 경로로 푼다. */
	FString TagToObjectPath(const FAssetData& Asset, const FName& Tag)
	{
		FString Value;
		return Asset.GetTagValue(Tag, Value) ? FPackageName::ExportTextPathToObjectPath(Value) : FString();
	}
}

void UNetAssetRetainerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const int32 Count = Retain();
	UE_LOG(LogMintChoco, Log, TEXT("NetAssetRetainer: %d objects retained for the game instance lifetime"), Count);
}

void UNetAssetRetainerSubsystem::Deinitialize()
{
	Retained.Empty();
	Super::Deinitialize();
}

TArray<const UClass*> UNetAssetRetainerSubsystem::GetRetainedAssetClasses()
{
	return {
		UItemProfile::StaticClass(),
		UPaintWeaponProfile::StaticClass(),
		UPaintballProfile::StaticClass(),
		UPaintScatterProfile::StaticClass(),
		UPaintBrushProfile::StaticClass(),
		UUnitDataAsset::StaticClass(),
		UUnitInputConfig::StaticClass(),
		USoundBank::StaticClass(),
	};
}

TArray<const UClass*> UNetAssetRetainerSubsystem::GetRetainedActorClasses()
{
	return {
		AUnit::StaticClass(),
		AItemPickup::StaticClass(),
	};
}

int32 UNetAssetRetainerSubsystem::Retain()
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	// 패키지 빌드는 미리 만든 레지스트리를 시작할 때 다 읽지만, 에디터는 아직 훑는 중일 수 있다.
	if (Registry.IsLoadingAssets())
	{
		Registry.WaitForCompletion();
	}

	// 1. 데이터 에셋: 복제 참조에 실리는 클래스의 모든 인스턴스.
	for (const UClass* const Class : GetRetainedAssetClasses())
	{
		TArray<FAssetData> Found;
		FindAssetsOfClass(Registry, Class->GetClassPathName(), Found);
		for (const FAssetData& Asset : Found)
		{
			RetainObject(Asset.GetAsset());
		}
	}

	// 2. 설정이 가리키는 것: 스폰 후보 목록과 픽업 클래스. 레지스트리 밖(플러그인, 엔진)에 있어도 잡힌다.
	const UItemSettings& ItemSettings = UItemSettings::Get();
	TArray<UItemProfile*> Items;
	ItemSettings.LoadItems(Items);
	for (UItemProfile* const Item : Items)
	{
		RetainObject(Item);
	}
	RetainObject(ItemSettings.LoadPickupClass());

	// 3. 서버가 스폰하는 액터의 블루프린트 클래스. 클래스 자체와 그 원형(Default__X_C)이 GUID로 오간다.
	// 쿠킹된 레지스트리에도 블루프린트 자산은 네이티브 부모·생성 클래스 태그를 그대로 지닌다.
	const TArray<const UClass*> ActorParents = GetRetainedActorClasses();
	TArray<FAssetData> Blueprints;
	FindAssetsOfClass(Registry, FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")), Blueprints);
	for (const FAssetData& Asset : Blueprints)
	{
		const FString NativeParentPath = TagToObjectPath(Asset, FBlueprintTags::NativeParentClassPath);
		const UClass* const NativeParent = NativeParentPath.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *NativeParentPath);
		if (!NativeParent)
		{
			continue;
		}

		const bool bWanted = ActorParents.ContainsByPredicate([NativeParent](const UClass* Parent)
		{
			return NativeParent->IsChildOf(Parent);
		});
		if (!bWanted)
		{
			continue;
		}

		const FString GeneratedPath = TagToObjectPath(Asset, FBlueprintTags::GeneratedClassPath);
		if (!GeneratedPath.IsEmpty())
		{
			RetainObject(FSoftClassPath(GeneratedPath).TryLoadClass<UObject>());
		}
	}

	return Retained.Num();
}

bool UNetAssetRetainerSubsystem::IsRetained(const UObject* Object) const
{
	return Object && Retained.Contains(const_cast<UObject*>(Object));
}

void UNetAssetRetainerSubsystem::RetainObject(UObject* Object)
{
	if (Object)
	{
		Retained.AddUnique(Object);
	}
}
