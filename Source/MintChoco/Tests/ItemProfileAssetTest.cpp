#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#include "Items/BeeProfile.h"
#include "Items/BeeProjectile.h"
#include "Items/ChocolateFountain.h"
#include "Items/ChocolateFountainProfile.h"
#include "Items/DessertBombardmentProfile.h"
#include "Items/HeroLandingProfile.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/HoneyBalloonProjectile.h"
#include "Items/ItemAbility.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "Items/PaintRain.h"
#include "Items/SpeedStarProfile.h"
#include "Items/SweetSpinnerProfile.h"
#include "Weapons/PaintGunProfile.h"
#include "Weapons/PaintballProfile.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* const ItemFolder = TEXT("/Game/Blueprints/Items");
}

/**
 * 출하되는 아이템 에셋이 "조용히 아무것도 안 하는" 상태가 아닌지 지킨다: 어빌리티 없는 아이템,
 * 산탄 없는 스피너, 칠하지 못하는 스피드 스타, 투사체·돔·탄이 빠진 새 아이템. 설정의 목록도
 * 전부 로드되어야 한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemProfileAssetTest,
	"MintChoco.Items.ProfileAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FItemProfileAssetTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ItemFolder}, /*bForceRescan=*/true);

	FARFilter Filter;
	Filter.ClassPaths.Add(UItemProfile::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(FName(ItemFolder));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Found;
	Registry.GetAssets(Filter, Found);

	if (!TestTrue(FString::Printf(TEXT("item profiles found under %s"), ItemFolder), Found.Num() > 0))
	{
		return false;
	}

	for (const FAssetData& Data : Found)
	{
		const FString Name = Data.AssetName.ToString();
		const UItemProfile* const Item = Cast<UItemProfile>(Data.GetAsset());
		if (!Item)
		{
			AddError(FString::Printf(TEXT("%s: failed to load as a UItemProfile."), *Name));
			continue;
		}

		TestNotNull(*FString::Printf(TEXT("%s: AbilityClass"), *Name), Item->AbilityClass.Get());
		TestTrue(*FString::Printf(TEXT("%s: Duration is not negative"), *Name), Item->Duration >= 0.0f);
		TestFalse(*FString::Printf(TEXT("%s: DisplayName"), *Name), Item->DisplayName.IsEmpty());
		// 즉발 아이템은 상태 태그가 없다. 지속형은 있어야 슬롯이 연출을 찾는다.
		if (!Item->IsInstant())
		{
			TestTrue(*FString::Printf(TEXT("%s: state tag resolves"), *Name), Item->GetStateTag().IsValid());
		}

		if (const USweetSpinnerProfile* const Spinner = Cast<USweetSpinnerProfile>(Item))
		{
			TestNotNull(*FString::Printf(TEXT("%s: Volley"), *Name), Spinner->Volley.Get());
			TestTrue(*FString::Printf(TEXT("%s: SpinRateDeg is positive"), *Name), Spinner->SpinRateDeg > 0.0f);
			TestTrue(*FString::Printf(TEXT("%s: VolleyInterval fits the duration"), *Name),
				Spinner->VolleyInterval > 0.0f && Spinner->VolleyInterval <= Spinner->Duration);
			if (Spinner->Volley)
			{
				TestNotNull(*FString::Printf(TEXT("%s: Volley paintball"), *Name), Spinner->Volley->Paintball.Get());
				TestNotNull(*FString::Printf(TEXT("%s: Volley scatter"), *Name), Spinner->Volley->Scatter.Get());
			}
		}
		else if (const USpeedStarProfile* const Star = Cast<USpeedStarProfile>(Item))
		{
			TestTrue(*FString::Printf(TEXT("%s: TrailDeposit can paint"), *Name), Star->TrailDeposit.CanPaint());
			TestTrue(*FString::Printf(TEXT("%s: MarkSpacing is positive"), *Name), Star->MarkSpacing > 0.0f);
		}
		else if (const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(Item))
		{
			TestTrue(*FString::Printf(TEXT("%s: instant"), *Name), Honey->IsInstant());
			TestNotNull(*FString::Printf(TEXT("%s: ProjectileClass"), *Name), Honey->ProjectileClass.Get());
			TestNotNull(*FString::Printf(TEXT("%s: Burst paintball"), *Name), Honey->Burst.Paintball.Get());
			TestTrue(*FString::Printf(TEXT("%s: Burst count"), *Name), Honey->Burst.Count > 0);
		}
		else if (const UChocolateFountainProfile* const Fountain = Cast<UChocolateFountainProfile>(Item))
		{
			TestTrue(*FString::Printf(TEXT("%s: instant"), *Name), Fountain->IsInstant());
			TestNotNull(*FString::Printf(TEXT("%s: DomeClass"), *Name), Fountain->DomeClass.Get());
			TestTrue(*FString::Printf(TEXT("%s: Lifetime is positive"), *Name), Fountain->Lifetime > 0.0f);
		}
		else if (const UHeroLandingProfile* const Landing = Cast<UHeroLandingProfile>(Item))
		{
			TestFalse(*FString::Printf(TEXT("%s: has a safety duration"), *Name), Landing->IsInstant());
			TestNotNull(*FString::Printf(TEXT("%s: Burst paintball"), *Name), Landing->Burst.Paintball.Get());
			TestTrue(*FString::Printf(TEXT("%s: RiseHeight is positive"), *Name), Landing->Landing.RiseHeight > 0.0f);
			TestTrue(*FString::Printf(TEXT("%s: duration outlasts rise and hover"), *Name),
				Landing->Duration > Landing->Landing.RiseTime + Landing->Landing.HoverTime);
		}
		else if (const UBeeProfile* const Bee = Cast<UBeeProfile>(Item))
		{
			TestTrue(*FString::Printf(TEXT("%s: instant"), *Name), Bee->IsInstant());
			TestNotNull(*FString::Printf(TEXT("%s: ProjectileClass"), *Name), Bee->ProjectileClass.Get());
			TestNotNull(*FString::Printf(TEXT("%s: Burst paintball"), *Name), Bee->Burst.Paintball.Get());
			TestTrue(*FString::Printf(TEXT("%s: TrailDeposit can paint"), *Name), Bee->TrailDeposit.CanPaint());
			TestTrue(*FString::Printf(TEXT("%s: Health is positive"), *Name), Bee->Health > 0.0f);
		}
		else if (const UDessertBombardmentProfile* const Bombardment = Cast<UDessertBombardmentProfile>(Item))
		{
			TestTrue(*FString::Printf(TEXT("%s: instant"), *Name), Bombardment->IsInstant());
			TestNotNull(*FString::Printf(TEXT("%s: Paintball"), *Name), Bombardment->Paintball.Get());
			TestTrue(*FString::Printf(TEXT("%s: Columns is positive"), *Name), Bombardment->Columns > 0);
		}
	}

	// 설정의 목록은 게임모드가 스폰할 것이고, 하나라도 못 읽으면 조용히 빠진다.
	TArray<UItemProfile*> Configured;
	UItemSettings::Get().LoadItems(Configured);
	TestEqual(TEXT("every configured item loads"), Configured.Num(), UItemSettings::Get().Items.Num());
	TestTrue(TEXT("at least one item configured"), Configured.Num() > 0);
	TestNotNull(TEXT("pickup class configured"), UItemSettings::Get().LoadPickupClass());

	return true;
}

#endif
