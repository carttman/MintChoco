#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FName OverlayPackage(TEXT("/Game/Assets/Paint/Materials/Overlay/MF_PaintOverlay"));
	const FName PaintMaxHeightParam(TEXT("PaintMaxHeight"));

	/**
	 * Every material that reaches the overlay function, master or instance, by climbing the
	 * reference graph up from it. By reference, never by folder: a paint material may live anywhere.
	 */
	void FindPaintMaterials(IAssetRegistry& Registry, TArray<UMaterialInterface*>& OutFound)
	{
		TSet<FName> Seen;
		TArray<FName> Pending;
		Seen.Add(OverlayPackage);
		Pending.Add(OverlayPackage);

		while (Pending.Num() > 0)
		{
			TArray<FName> Referencers;
			Registry.GetReferencers(Pending.Pop(), Referencers);
			for (const FName& Package : Referencers)
			{
				bool bAlreadySeen = false;
				Seen.Add(Package, &bAlreadySeen);
				if (bAlreadySeen)
				{
					continue;
				}
				TArray<FAssetData> Assets;
				Registry.GetAssetsByPackageName(Package, Assets);
				bool bHoldsMaterial = false;
				for (const FAssetData& Data : Assets)
				{
					// The class is read before the asset so a level or a mesh referencing a paint
					// material is skipped instead of loaded.
					const UClass* const Class = Data.GetClass();
					if (!Class || !Class->IsChildOf(UMaterialInterface::StaticClass()))
					{
						continue;
					}
					bHoldsMaterial = true;
					if (UMaterialInterface* const Material = Cast<UMaterialInterface>(Data.GetAsset()))
					{
						OutFound.Add(Material);
					}
				}
				// Only a material passes the parameter on to its instances; anything else is a dead end.
				if (bHoldsMaterial)
				{
					Pending.Add(Package);
				}
			}
		}
	}
}

/**
 * Nanite raises the surface by DisplacementScaling.Magnitude while the shading normal slopes a
 * texel by PaintMaxHeight: two stores of one number, in world cm. At play UPaintableComponent
 * derives the parameter from the magnitude, so this guards what it cannot reach - the editor
 * viewport, a material preview, and the defaults a new instance starts from. A failure is fixed
 * by moving PaintMaxHeight onto the magnitude, never the other way around.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintHeightTest,
	"MintChoco.Paint.Materials.PaintHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintHeightTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	// The reference graph is only complete once the first scan is.
	Registry.WaitForCompletion();

	TArray<UMaterialInterface*> Materials;
	FindPaintMaterials(Registry, Materials);
	// An empty result would pass every assertion below without testing anything.
	if (!TestTrue(TEXT("materials reaching MF_PaintOverlay found"), Materials.Num() > 0))
	{
		return false;
	}

	int32 Declaring = 0;
	for (const UMaterialInterface* const Material : Materials)
	{
		float MaxHeight = 0.0f;
		if (!Material->GetScalarParameterValue(FMaterialParameterInfo(PaintMaxHeightParam), MaxHeight))
		{
			continue;
		}
		++Declaring;
		TestEqual(
			*FString::Printf(TEXT("%s: PaintMaxHeight matches DisplacementScaling.Magnitude"), *Material->GetName()),
			MaxHeight,
			Material->GetDisplacementScaling().Magnitude);
	}
	TestTrue(TEXT("a paint material declares PaintMaxHeight"), Declaring > 0);
	return true;
}

#endif
