#include "Misc/AutomationTest.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintGunProfile.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintSniperProfile.h"
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
		TestTrue(*FString::Printf(TEXT("%s: HeightAdd is positive"), *Name), Deposit.GetHeightAdd() > 0.0f);
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
		if (Scatter->Pattern == EPaintScatterPattern::HorizontalFan)
		{
			// A fan of one pellet or of zero width is a cone in disguise; it should say so.
			TestTrue(*FString::Printf(TEXT("%s: a fan needs several pellets"), *Name), Scatter->PelletsPerShot >= 2);
			TestTrue(*FString::Printf(TEXT("%s: FanHalfAngleDeg is positive"), *Name), Scatter->FanHalfAngleDeg > 0.0f);
		}
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
		if (Weapon->FireMode == EPaintFireMode::Charged)
		{
			TestTrue(*FString::Printf(TEXT("%s: ChargeTime is positive"), *Name), Weapon->ChargeTime > 0.0f);
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
		else if (const UPaintSniperProfile* const Sniper = Cast<UPaintSniperProfile>(Weapon))
		{
			TestTrue(
				*FString::Printf(TEXT("%s: a sniper in Continuous lays a whole trail per frame; use Charged or Single"), *Name),
				Sniper->FireMode != EPaintFireMode::Continuous);
			CheckDeposit(Name + TEXT(" Impact"), Sniper->Impact);
			CheckDeposit(Name + TEXT(" Trail"), Sniper->Trail);
			TestTrue(*FString::Printf(TEXT("%s: Range is positive"), *Name), Sniper->Range > 0.0f);
			// A spacing of zero never advances along the ray.
			TestTrue(*FString::Printf(TEXT("%s: TrailSpacing is positive"), *Name), Sniper->TrailSpacing > 0.0f);
			TestTrue(*FString::Printf(TEXT("%s: TrailDropHeight is positive"), *Name), Sniper->TrailDropHeight > 0.0f);
		}
	}

	return true;
}

/**
 * 충전량이 총구 연출의 크기를 정하는 방식. 최소 충전에서 아래끝, 풀충전에서 위끝이고,
 * 풀충전만 발사하는 프로필(MinChargeToFire 1)은 위끝 하나만 쓴다. 차지가 아닌 무기는 배율이 고정이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintMuzzleFXScaleTest,
	"MintChoco.Paint.Weapons.MuzzleFXScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintMuzzleFXScaleTest::RunTest(const FString& Parameters)
{
	UPaintSniperProfile* const Charged = NewObject<UPaintSniperProfile>();
	Charged->FireMode = EPaintFireMode::Charged;
	Charged->MinChargeToFire = 0.3f;
	Charged->MuzzleFXScale = 1.0f;
	Charged->MuzzleFXChargeScale = FVector2D(0.5, 1.5);

	TestEqual(TEXT("minimum charge is the low end"), Charged->GetMuzzleFXScale(0.3f), 0.5f, 1e-4f);
	TestEqual(TEXT("full charge is the high end"), Charged->GetMuzzleFXScale(1.0f), 1.5f, 1e-4f);
	TestEqual(TEXT("halfway between is halfway"), Charged->GetMuzzleFXScale(0.65f), 1.0f, 1e-4f);
	TestEqual(TEXT("below the minimum clamps to the low end"), Charged->GetMuzzleFXScale(0.0f), 0.5f, 1e-4f);

	// 기본 배율은 곱해진다: 에셋이 두 배로 크면 두 끝도 두 배다.
	Charged->MuzzleFXScale = 2.0f;
	TestEqual(TEXT("MuzzleFXScale multiplies the charge scale"), Charged->GetMuzzleFXScale(1.0f), 3.0f, 1e-4f);

	// 풀충전만 발사하면 도달 가능한 크기는 위끝 하나다.
	Charged->MuzzleFXScale = 1.0f;
	Charged->MinChargeToFire = 1.0f;
	TestEqual(TEXT("a full-charge-only weapon always uses the high end"), Charged->GetMuzzleFXScale(1.0f), 1.5f, 1e-4f);

	// 산탄 패턴(UPaintScatterProfile)은 무기 프로필이 아니다. 샷건이 드는 것은 건 프로필이다.
	UPaintGunProfile* const Single = NewObject<UPaintGunProfile>();
	Single->FireMode = EPaintFireMode::Single;
	Single->MuzzleFXScale = 1.25f;
	Single->MuzzleFXChargeScale = FVector2D(0.5, 1.5);
	TestEqual(TEXT("a non-charged weapon ignores the charge range"), Single->GetMuzzleFXScale(1.0f), 1.25f, 1e-4f);
	TestEqual(TEXT("a non-charged weapon ignores the charge value"), Single->GetMuzzleFXScale(0.0f), 1.25f, 1e-4f);

	return true;
}

/**
 * 충전 연출은 차지 무기에서만 의미가 있다. 차지가 아닌 프로필에 ChargeFX가 들어 있으면
 * 영원히 재생되지 않는 에셋 참조이므로, 값을 넣은 쪽의 FireMode가 Charged인지 본다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintChargeFXProfileTest,
	"MintChoco.Paint.Weapons.ChargeFXProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintChargeFXProfileTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ProfileFolder}, /*bForceRescan=*/true);

	TArray<FAssetData> Profiles;
	FindProfileAssets(Registry, UPaintWeaponProfile::StaticClass(), Profiles);
	TestTrue(TEXT("profile templates were found"), Profiles.Num() > 0);

	for (const FAssetData& Asset : Profiles)
	{
		const UPaintWeaponProfile* const Profile = Cast<UPaintWeaponProfile>(Asset.GetAsset());
		if (!Profile || !Profile->ChargeFX)
		{
			continue;
		}
		TestEqual(
			*FString::Printf(TEXT("%s: ChargeFX is only reachable in Charged"), *Asset.AssetName.ToString()),
			Profile->FireMode, EPaintFireMode::Charged);
		TestTrue(
			*FString::Printf(TEXT("%s: ChargeFXScale is positive"), *Asset.AssetName.ToString()),
			Profile->ChargeFXScale > 0.0f);
	}
	return true;
}

#endif
