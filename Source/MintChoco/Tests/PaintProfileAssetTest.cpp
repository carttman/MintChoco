#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintGunProfile.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintStrokeProfile.h"
#include "Weapons/PaintWeaponProfile.h"
#include "Weapons/PaintballProfile.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TCHAR* const ProfileFolder = TEXT("/Game/Blueprints/Weapons");

	/** Every asset of the class (or a subclass) under the profile folder. By class, never by a name pattern: a renamed asset must not drop out of coverage silently. */
	void FindProfileAssets(IAssetRegistry& Registry, const UClass* Class, TArray<FAssetData>& OutFound)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(FName(ProfileFolder));
		Filter.bRecursivePaths = true;
		Registry.GetAssets(Filter, OutFound);
	}
}

/**
 * Guards the shipped profile templates against the settings that fail silently: a deposit that
 * cannot paint or shows nothing, a gun missing half of itself, or one that spawns a ball every frame.
 * Every template is checked on its own, since a designer may pick any of them for a new pairing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintProfileAssetTest,
	"MintChoco.Paint.Weapons.ProfileAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintProfileAssetTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	// A commandlet run may not have finished its first scan yet.
	Registry.ScanPathsSynchronous({ProfileFolder}, /*bForceRescan=*/true);

	const auto CheckDeposit = [this](const FString& Name, const FPaintDeposit& Deposit)
	{
		TestNotNull(*FString::Printf(TEXT("%s: BrushProfile"), *Name), Deposit.BrushProfile.Get());
		TestTrue(*FString::Printf(TEXT("%s: SplatVolume is positive"), *Name), Deposit.SplatVolume > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s: HeightAdd is positive"), *Name), Deposit.HeightAdd > 0.0f);
	};

	TArray<FAssetData> Paintballs;
	FindProfileAssets(Registry, UPaintballProfile::StaticClass(), Paintballs);
	TArray<FAssetData> Scatters;
	FindProfileAssets(Registry, UPaintScatterProfile::StaticClass(), Scatters);
	TArray<FAssetData> Weapons;
	FindProfileAssets(Registry, UPaintWeaponProfile::StaticClass(), Weapons);

	// An empty result would pass every assertion below without testing anything.
	const bool bHavePaintballs = TestTrue(FString::Printf(TEXT("paintball profiles found under %s"), ProfileFolder), Paintballs.Num() > 0);
	const bool bHaveScatters = TestTrue(FString::Printf(TEXT("scatter profiles found under %s"), ProfileFolder), Scatters.Num() > 0);
	const bool bHaveWeapons = TestTrue(FString::Printf(TEXT("weapon profiles found under %s"), ProfileFolder), Weapons.Num() > 0);
	if (!bHavePaintballs || !bHaveScatters || !bHaveWeapons)
	{
		return false;
	}

	for (const FAssetData& Data : Paintballs)
	{
		const FString Name = Data.AssetName.ToString();
		const UPaintballProfile* const Paintball = Cast<UPaintballProfile>(Data.GetAsset());
		if (!Paintball)
		{
			AddError(FString::Printf(TEXT("%s: failed to load as a UPaintballProfile."), *Name));
			continue;
		}
		TestNotNull(*FString::Printf(TEXT("%s: ProjectileClass"), *Name), Paintball->ProjectileClass.Get());
		TestTrue(*FString::Printf(TEXT("%s: Radius is positive"), *Name), Paintball->Radius > 0.0f);
		CheckDeposit(Name, Paintball->Deposit);
	}

	for (const FAssetData& Data : Scatters)
	{
		const FString Name = Data.AssetName.ToString();
		const UPaintScatterProfile* const Scatter = Cast<UPaintScatterProfile>(Data.GetAsset());
		if (!Scatter)
		{
			AddError(FString::Printf(TEXT("%s: failed to load as a UPaintScatterProfile."), *Name));
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s: MuzzleSpeed is positive"), *Name), Scatter->MuzzleSpeed > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s: PelletsPerShot is at least one"), *Name), Scatter->PelletsPerShot >= 1);
	}

	for (const FAssetData& Data : Weapons)
	{
		const FString Name = Data.AssetName.ToString();
		const UPaintWeaponProfile* const Weapon = Cast<UPaintWeaponProfile>(Data.GetAsset());
		if (!Weapon)
		{
			AddError(FString::Printf(TEXT("%s: failed to load as a UPaintWeaponProfile."), *Name));
			continue;
		}

		if (Weapon->FireMode == EPaintFireMode::Automatic)
		{
			TestTrue(*FString::Printf(TEXT("%s: ShotsPerSecond is in range"), *Name),
				Weapon->ShotsPerSecond >= 0.1f && Weapon->ShotsPerSecond <= 60.0f);
		}

		if (const UPaintGunProfile* const Gun = Cast<UPaintGunProfile>(Weapon))
		{
			TestTrue(
				*FString::Printf(TEXT("%s: a gun in Continuous spawns a ball per frame; use Automatic with a rate"), *Name),
				Gun->FireMode != EPaintFireMode::Continuous);
			TestNotNull(*FString::Printf(TEXT("%s: Paintball"), *Name), Gun->Paintball.Get());
			TestNotNull(*FString::Printf(TEXT("%s: Scatter"), *Name), Gun->Scatter.Get());
			TestTrue(*FString::Printf(TEXT("%s: AimTraceDistance is positive"), *Name), Gun->AimTraceDistance > 0.0f);
		}
		else if (const UPaintStrokeProfile* const Stroke = Cast<UPaintStrokeProfile>(Weapon))
		{
			CheckDeposit(Name, Stroke->Deposit);
			TestTrue(*FString::Printf(TEXT("%s: Reach is positive"), *Name), Stroke->Reach > 0.0f);
			// A spacing of zero costs a full-target draw every tick of the stroke.
			TestTrue(*FString::Printf(TEXT("%s: StrokeSpacing is positive"), *Name), Stroke->StrokeSpacing > 0.0f);
		}
	}

	return true;
}

#endif
