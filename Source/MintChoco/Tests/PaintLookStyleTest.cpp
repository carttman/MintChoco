#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Modules/ModuleManager.h"

#include "Paint/PaintSettings.h"
#include "Paint/PaintSubsystem.h"
#include "Tests/TestWorld.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FName TeamLookPackage(TEXT("/Game/Assets/Paint/Materials/Team/MF_TeamLook"));
	const TCHAR* const TeamLookPath = TEXT("/Game/Assets/Paint/Materials/Team/MF_TeamLook.MF_TeamLook");
	const TCHAR* const PaintStylePath = TEXT("/Game/Assets/Paint/Materials/Style/MF_PaintStyle.MF_PaintStyle");

	/**
	 * Opts a material out of the style. A graybox or test material is tinted by team without
	 * belonging to the look, so a look sweep has no business reaching it. Declared as an unwired
	 * scalar so the intent is visible to whoever opens the asset, and inherited by its instances.
	 */
	const FName ExemptParam(TEXT("PaintStyleExempt"));

	/** The two MPC_PaintStyle entries, spelled as UPaintSubsystem::SetLookStyle spells them. */
	const FName StylePackedParameter(TEXT("Style"));
	const FName StyleExtraParameter(TEXT("Style2"));

	/**
	 * Every material that reaches the team look function, master or instance, by climbing the
	 * reference graph up from it. By reference, never by folder: a paint material may live anywhere.
	 *
	 * A material's package serialises a hard pointer to every function it uses transitively, so a
	 * master whose team look sits behind a layer stack arrives in the same hop as a direct caller.
	 */
	void FindTeamLookMaterials(IAssetRegistry& Registry, TArray<UMaterialInterface*>& OutFound)
	{
		TSet<FName> Seen;
		TArray<FName> Pending;
		Seen.Add(TeamLookPackage);
		Pending.Add(TeamLookPackage);

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
				// Only a material passes the function on to its instances; anything else is a dead end.
				if (bHoldsMaterial)
				{
					Pending.Add(Package);
				}
			}
		}
	}

	/**
	 * Whether the material really calls the function. The reference graph answers "mentions",
	 * which a package can do for reasons of its own; this reads the live graph, walks layer
	 * stacks and nested functions, and is the engine's own answer to the same question.
	 */
	bool UsesFunction(const UMaterialInterface& Material, UMaterialFunctionInterface* Function)
	{
		TArray<UMaterialFunctionInterface*> Dependents;
		Material.GetDependentFunctions(Dependents);
		return Dependents.Contains(Function);
	}

	bool IsExempt(const UMaterialInterface& Material)
	{
		float Exempt = 0.0f;
		return Material.GetScalarParameterValue(FMaterialParameterInfo(ExemptParam), Exempt) && Exempt != 0.0f;
	}

	void GatherCollectionReads(
		TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,
		const UMaterialParameterCollection* Collection,
		TSet<FName>& OutRead)
	{
		for (const TObjectPtr<UMaterialExpression>& Expression : Expressions)
		{
			const UMaterialExpressionCollectionParameter* const Read =
				Cast<UMaterialExpressionCollectionParameter>(Expression.Get());
			if (Read && Read->Collection.Get() == Collection)
			{
				OutRead.Add(Read->ParameterName);
			}
		}
	}

	/** The in-scope set: tinted by the team look, and not opted out. */
	void FindStyledMaterials(
		IAssetRegistry& Registry,
		UMaterialFunctionInterface* TeamLook,
		TArray<UMaterialInterface*>& OutStyled,
		int32& OutReached,
		int32& OutExempt)
	{
		TArray<UMaterialInterface*> Materials;
		FindTeamLookMaterials(Registry, Materials);
		OutReached = Materials.Num();
		OutExempt = 0;

		for (UMaterialInterface* const Material : Materials)
		{
			if (!UsesFunction(*Material, TeamLook))
			{
				continue;
			}
			if (IsExempt(*Material))
			{
				++OutExempt;
				continue;
			}
			OutStyled.Add(Material);
		}
	}
}

/**
 * Every material the team look tints must run its look through MF_PaintStyle, so that
 * mc.Paint.Style reaches all of them at once. A material that calls MF_TeamLook raw and applies
 * its own coat or roughness renders the pre-sweep look forever, and nothing on screen says so -
 * the side splat drifted that way for a whole commit, at coat 1.0 against the floor's 0.66.
 *
 * A material that is tinted by team without belonging to the look (graybox, a test asset) says so
 * with a PaintStyleExempt scalar rather than being listed here, so this test never needs editing
 * when an asset is added or moved.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintLookStyleTest,
	"MintChoco.Paint.Materials.LookStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintLookStyleTest::RunTest(const FString& Parameters)
{
	UMaterialFunctionInterface* const TeamLook = LoadObject<UMaterialFunctionInterface>(nullptr, TeamLookPath);
	UMaterialFunctionInterface* const PaintStyle = LoadObject<UMaterialFunctionInterface>(nullptr, PaintStylePath);
	if (!TestNotNull(TEXT("MF_TeamLook loads"), TeamLook) || !TestNotNull(TEXT("MF_PaintStyle loads"), PaintStyle))
	{
		return false;
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	// The reference graph is only complete once the first scan is.
	Registry.WaitForCompletion();

	TArray<UMaterialInterface*> Styled;
	int32 Reached = 0;
	int32 Exempt = 0;
	FindStyledMaterials(Registry, TeamLook, Styled, Reached, Exempt);
	AddInfo(FString::Printf(TEXT("%d materials reach MF_TeamLook; %d in scope, %d exempt."), Reached, Styled.Num(), Exempt));

	// An empty result would pass every assertion below without testing anything.
	if (!TestTrue(TEXT("a team-tinted material is in scope"), Styled.Num() > 0))
	{
		return false;
	}

	for (const UMaterialInterface* const Material : Styled)
	{
		// An instance only ever follows its master here - the one way it can differ is a layer
		// stack override, which is itself worth reporting - so the master is named alongside it
		// and a run of failures points at the handful of graphs that actually need the wire.
		const UMaterial* const Base = Material->GetMaterial();
		const FString Where = Base && Base != Material
			? FString::Printf(TEXT("%s (through %s)"), *Material->GetName(), *Base->GetName())
			: Material->GetName();
		TestTrue(
			*FString::Printf(TEXT("%s: takes its look through MF_PaintStyle"), *Where),
			UsesFunction(*Material, PaintStyle));
	}
	return true;
}

/**
 * Every entry of MPC_PaintStyle has to be read by something. An entry nobody reads is a knob that
 * turns nothing: Style2 (NormalStrength) was written by SetLookStyle and read by no material at
 * all, so mc.Paint.Style's last argument moved a number in memory and changed no pixel.
 *
 * Reads are counted across the whole in-scope set and every function it depends on, because a
 * single consumer inside MF_PaintHeightField or MF_PaintOverlay is a perfectly good home for an
 * axis that only one part of the pipeline can use.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintLookStyleCollectionTest,
	"MintChoco.Paint.Materials.LookStyleCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintLookStyleCollectionTest::RunTest(const FString& Parameters)
{
	const UMaterialParameterCollection* const Collection = UPaintSettings::Get().StyleCollection.LoadSynchronous();
	if (!TestNotNull(TEXT("PaintSettings.StyleCollection loads"), Collection))
	{
		return false;
	}

	UMaterialFunctionInterface* const TeamLook = LoadObject<UMaterialFunctionInterface>(nullptr, TeamLookPath);
	if (!TestNotNull(TEXT("MF_TeamLook loads"), TeamLook))
	{
		return false;
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();

	TArray<UMaterialInterface*> Styled;
	int32 Reached = 0;
	int32 Exempt = 0;
	FindStyledMaterials(Registry, TeamLook, Styled, Reached, Exempt);
	if (!TestTrue(TEXT("a team-tinted material is in scope"), Styled.Num() > 0))
	{
		return false;
	}

	TSet<FName> Read;
	for (const UMaterialInterface* const Material : Styled)
	{
		if (const UMaterial* const Base = Material->GetMaterial())
		{
			GatherCollectionReads(Base->GetExpressions(), Collection, Read);
		}
		TArray<UMaterialFunctionInterface*> Dependents;
		Material->GetDependentFunctions(Dependents);
		for (const UMaterialFunctionInterface* const Function : Dependents)
		{
			GatherCollectionReads(Function->GetExpressions(), Collection, Read);
		}
	}

	for (const FCollectionVectorParameter& Entry : Collection->VectorParameters)
	{
		TestTrue(
			*FString::Printf(TEXT("%s: a paint material reads it"), *Entry.ParameterName.ToString()),
			Read.Contains(Entry.ParameterName));
	}
	for (const FCollectionScalarParameter& Entry : Collection->ScalarParameters)
	{
		TestTrue(
			*FString::Printf(TEXT("%s: a paint material reads it"), *Entry.ParameterName.ToString()),
			Read.Contains(Entry.ParameterName));
	}
	return true;
}

/**
 * The style mirror starts where the shaders already are. UPaintSubsystem keeps a copy of the style
 * so mc.Paint.Style can change one axis and leave the rest alone; seeded from the struct's own
 * defaults rather than the collection's, that very first partial command writes the struct over
 * the asset - name only the coat and fuzz 1.3, bias 0.12 and flow 0.33 go with it, silently.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintLookStyleSeedTest,
	"MintChoco.Paint.Style.Seed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPaintLookStyleSeedTest::RunTest(const FString& Parameters)
{
	const UMaterialParameterCollection* const Collection = UPaintSettings::Get().StyleCollection.LoadSynchronous();
	if (!TestNotNull(TEXT("PaintSettings.StyleCollection loads"), Collection))
	{
		return false;
	}
	const FCollectionVectorParameter* const Packed = Collection->GetVectorParameterByName(StylePackedParameter);
	const FCollectionVectorParameter* const Extra = Collection->GetVectorParameterByName(StyleExtraParameter);
	if (!TestNotNull(TEXT("Style entry"), Packed) || !TestNotNull(TEXT("Style2 entry"), Extra))
	{
		return false;
	}

	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	if (const UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>())
	{
		const FPaintLookStyle& Style = Paint->GetLookStyle();
		TestEqual(TEXT("CoatScale seeded from the collection"), Style.CoatScale, Packed->DefaultValue.R);
		TestEqual(TEXT("FuzzScale seeded from the collection"), Style.FuzzScale, Packed->DefaultValue.G);
		TestEqual(TEXT("RoughnessBias seeded from the collection"), Style.RoughnessBias, Packed->DefaultValue.B);
		TestEqual(TEXT("Flow seeded from the collection"), Style.Flow, Packed->DefaultValue.A);
		TestEqual(TEXT("NormalStrength seeded from the collection"), Style.NormalStrength, Extra->DefaultValue.R);
	}
	else
	{
		AddError(TEXT("the test world has no paint subsystem"));
	}
	MintChocoTest::DestroyWorld(World);
	return true;
}

#endif
