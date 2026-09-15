#include "Misc/AutomationTest.h"

#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

#include "Look/LookPreset.h"
#include "Look/LookSettings.h"
#include "Look/LookSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

/** mc.Look 인자: 끄기, 1부터 세는 번호, 대소문자 무시 이름. 나머지는 거절. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLookArgumentTest,
	"MintChoco.Look.Argument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLookArgumentTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = {TEXT("SoftPBR"), TEXT("Hybrid"), TEXT("Toon")};

	struct FCase
	{
		const TCHAR* Argument;
		bool bValid;
		int32 Index;
	};
	const FCase Cases[] = {
		{TEXT("Off"), true, INDEX_NONE},
		{TEXT("baseline"), true, INDEX_NONE},
		{TEXT("0"), true, INDEX_NONE},
		{TEXT("1"), true, 0},
		{TEXT("3"), true, 2},
		{TEXT("4"), false, INDEX_NONE},
		{TEXT("-1"), false, INDEX_NONE},
		{TEXT("toon"), true, 2},
		{TEXT(" HYBRID "), true, 1},
		{TEXT("Cel"), false, INDEX_NONE},
		{TEXT(""), false, INDEX_NONE},
	};
	for (const FCase& Case : Cases)
	{
		int32 Index = 123;
		const bool bValid = LookPreset::ResolveArgument(Case.Argument, Names, Index);
		TestEqual(*FString::Printf(TEXT("'%s' is accepted"), Case.Argument), bValid, Case.bValid);
		if (Case.bValid)
		{
			TestEqual(*FString::Printf(TEXT("'%s' index"), Case.Argument), Index, Case.Index);
		}
	}
	return true;
}

/** 콘솔 변수는 처음 값만 기억하고, 없는 변수는 거절하고, 되돌리면 원래 값으로 돌아간다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLookConsoleVariableBackupTest,
	"MintChoco.Look.ConsoleVariableBackup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLookConsoleVariableBackupTest::RunTest(const FString& Parameters)
{
	IConsoleManager& Manager = IConsoleManager::Get();
	const TCHAR* const Name = TEXT("mc.Look.TestVariable");
	IConsoleVariable* const Variable = Manager.RegisterConsoleVariable(Name, 7, TEXT("MintChoco.Look.ConsoleVariableBackup 테스트 전용."), ECVF_Default);
	if (!TestNotNull(TEXT("test variable registers"), Variable))
	{
		return false;
	}

	FLookConsoleVariableBackup Backup;
	TestTrue(TEXT("a known variable is set"), Backup.Set(Name, TEXT("0")));
	TestEqual(TEXT("the value changed"), Variable->GetInt(), 0);
	TestTrue(TEXT("setting it again is fine"), Backup.Set(Name, TEXT("3")));
	TestFalse(TEXT("an unknown variable is refused"), Backup.Set(TEXT("mc.Look.NoSuchVariable"), TEXT("1")));

	Backup.RestoreAll();
	TestEqual(TEXT("the first original value comes back"), Variable->GetInt(), 7);
	TestTrue(TEXT("nothing is remembered after restoring"), Backup.IsEmpty());

	Manager.UnregisterConsoleObject(Name, false);
	return true;
}

/**
 * 설정에 걸린 프리셋이 전부 불러와지고, 이름이 겹치지 않고, 후처리 머티리얼은 PostProcess 도메인,
 * 스왑 대상은 스켈레탈 메시에서 쓸 수 있고, 콘솔 변수는 실제로 있는지.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLookPresetsTest,
	"MintChoco.Look.Presets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLookPresetsTest::RunTest(const FString& Parameters)
{
	const ULookSettings& Settings = ULookSettings::Get();
	TestTrue(TEXT("at least one preset is configured"), Settings.Presets.Num() > 0);
	TestNotNull(TEXT("SkyDomeMaterial loads"), Settings.SkyDomeMaterial.LoadSynchronous());

	TSet<FName> SeenNames;
	for (const TSoftObjectPtr<ULookPreset>& Pointer : Settings.Presets)
	{
		const ULookPreset* const Preset = Pointer.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *Pointer.ToString()), Preset))
		{
			continue;
		}
		const FString Label = Preset->GetName();
		TestFalse(*FString::Printf(TEXT("%s: has a ShortName"), *Label), Preset->ShortName.IsNone());
		TestFalse(*FString::Printf(TEXT("%s: ShortName is unique"), *Label), SeenNames.Contains(Preset->ShortName));
		SeenNames.Add(Preset->ShortName);

		for (const FWeightedBlendable& Blendable : Preset->PostProcess.WeightedBlendables.Array)
		{
			const UMaterialInterface* const Material = Cast<UMaterialInterface>(Blendable.Object);
			if (TestNotNull(*FString::Printf(TEXT("%s: post process blendable is a material"), *Label), Material))
			{
				TestEqual(*FString::Printf(TEXT("%s: %s is a post process material"), *Label, *Material->GetName()),
					static_cast<int32>(Material->GetMaterial()->MaterialDomain), static_cast<int32>(MD_PostProcess));
			}
		}

		for (const FLookMaterialSwap& Swap : Preset->MaterialSwaps)
		{
			TestNotNull(*FString::Printf(TEXT("%s: swap source %s loads"), *Label, *Swap.From.ToString()), Swap.From.LoadSynchronous());
			const UMaterialInterface* const To = Swap.To.LoadSynchronous();
			if (TestNotNull(*FString::Printf(TEXT("%s: swap target %s loads"), *Label, *Swap.To.ToString()), To))
			{
				TestTrue(*FString::Printf(TEXT("%s: %s is usable on skeletal meshes"), *Label, *To->GetName()), To->GetMaterial()->GetUsageByFlag(MATUSAGE_SkeletalMesh));
			}
		}

		for (const FLookConsoleVariable& Variable : Preset->ConsoleVariables)
		{
			TestNotNull(*FString::Printf(TEXT("%s: console variable %s exists"), *Label, *Variable.Name),
				IConsoleManager::Get().FindConsoleVariable(*Variable.Name));
		}
	}
	return true;
}

#endif
