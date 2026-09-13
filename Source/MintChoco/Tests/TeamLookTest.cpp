#include "Misc/AutomationTest.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialParameterCollection.h"

#include "Game/TeamLook.h"
#include "Game/TeamTypes.h"
#include "Paint/PaintSettings.h"
#include "Paint/PaintSideSplat.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool IsUnit(float Value)
	{
		return Value >= 0.0f && Value <= 1.0f;
	}
}

/**
 * MPC_TeamLook 이 프로젝트 설정에 걸려 있고 팀마다 세 항목을 다 갖는지. 항목 하나가 빠지면
 * 셰이더는 검정, C++ 는 내장값을 써서 화면과 표면이 조용히 어긋난다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTeamLookCollectionTest,
	"MintChoco.Game.TeamLook.Collection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTeamLookCollectionTest::RunTest(const FString& Parameters)
{
	const UMaterialParameterCollection* const Collection = UPaintSettings::Get().TeamLookCollection.LoadSynchronous();
	if (!TestNotNull(TEXT("PaintSettings.TeamLookCollection loads"), Collection))
	{
		return false;
	}

	for (int32 Team = 0; Team < Teams::Count; ++Team)
	{
		const FString Name = Teams::GetInternalName(Team);
		TestNotNull(*FString::Printf(TEXT("%s: Color entry"), *Name), Collection->GetVectorParameterByName(TeamLook::ColorParameterName(Team)));
		TestNotNull(*FString::Printf(TEXT("%s: Subsurface entry"), *Name), Collection->GetVectorParameterByName(TeamLook::SubsurfaceParameterName(Team)));
		const FCollectionVectorParameter* const Surface = Collection->GetVectorParameterByName(TeamLook::SurfaceParameterName(Team));
		if (TestNotNull(*FString::Printf(TEXT("%s: Surface entry"), *Name), Surface))
		{
			const FLinearColor& S = Surface->DefaultValue;
			TestTrue(*FString::Printf(TEXT("%s: Surface components are in 0..1"), *Name), IsUnit(S.R) && IsUnit(S.G) && IsUnit(S.B) && IsUnit(S.A));
		}

		// 접근자가 내장값이 아니라 에셋을 읽는지: 값이 에셋과 같아야 한다.
		const FTeamLook Look = TeamLook::Get(Team);
		if (const FCollectionVectorParameter* const Color = Collection->GetVectorParameterByName(TeamLook::ColorParameterName(Team)))
		{
			TestTrue(*FString::Printf(TEXT("%s: TeamLook::Get reads the asset color"), *Name), Look.Color.Equals(Color->DefaultValue));
		}
	}

	TestFalse(TEXT("teams have different colors"),
		TeamLook::GetFrom(Collection, Teams::Mint).Color.Equals(TeamLook::GetFrom(Collection, Teams::Choco).Color));
	return true;
}

/** 컬렉션 없이도 두 팀이 구분되고, 팀이 아닌 id 는 중립으로 떨어지는지. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTeamLookFallbackTest,
	"MintChoco.Game.TeamLook.Fallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTeamLookFallbackTest::RunTest(const FString& Parameters)
{
	const FTeamLook Mint = TeamLook::GetFrom(nullptr, Teams::Mint);
	const FTeamLook Choco = TeamLook::GetFrom(nullptr, Teams::Choco);
	TestFalse(TEXT("fallback colors differ between teams"), Mint.Color.Equals(Choco.Color));

	const FTeamLook Nobody = TeamLook::GetFrom(nullptr, 5);
	TestTrue(TEXT("an invalid id is neutral grey"), Nobody.Color.R == Nobody.Color.G && Nobody.Color.G == Nobody.Color.B);
	TestEqual(TEXT("display color of no team is Silver"), TeamLook::GetDisplayColor(Teams::None), FColor::Silver);

	TestEqual(TEXT("MintColor"), TeamLook::ColorParameterName(Teams::Mint), FName(TEXT("MintColor")));
	TestEqual(TEXT("ChocoSurface"), TeamLook::SurfaceParameterName(Teams::Choco), FName(TEXT("ChocoSurface")));
	TestEqual(TEXT("MintSubsurface"), TeamLook::SubsurfaceParameterName(Teams::Mint), FName(TEXT("MintSubsurface")));
	TestEqual(TEXT("no team has no entry name"), TeamLook::ColorParameterName(Teams::None), FName(NAME_None));
	return true;
}

namespace
{
	bool ReadScalar(const UMaterialInstance& Instance, FName Name, float& OutValue)
	{
		return Instance.GetScalarParameterValue(FHashedMaterialParameterInfo(Name), OutValue);
	}
}

/**
 * 팀별 MI 는 TeamId 하나로 팀을 고르고, 팀이 아닌 룩(무한탄 빨강)은 UseTeamLook 을 꺼야 한다.
 * TeamId 가 빠지면 두 팀이 같은 색이 되고, UseTeamLook 이 켜져 있으면 빨강이 팀 색으로 덮인다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTeamLookInstancesTest,
	"MintChoco.Game.TeamLook.Instances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTeamLookInstancesTest::RunTest(const FString& Parameters)
{
	struct FTeamInstance
	{
		const TCHAR* Path;
		int32 Team;
	};
	const FTeamInstance TeamInstances[] = {
		{TEXT("/Game/Assets/InkBall/Materials/MI_InkBall_Mint.MI_InkBall_Mint"), Teams::Mint},
		{TEXT("/Game/Assets/InkBall/Materials/MI_InkBall_Choco.MI_InkBall_Choco"), Teams::Choco},
		{TEXT("/Game/Assets/InkBottle/Materials/Liquid/MI_InkLiquid_Mint.MI_InkLiquid_Mint"), Teams::Mint},
		{TEXT("/Game/Assets/InkBottle/Materials/Liquid/MI_InkLiquid_Choco.MI_InkLiquid_Choco"), Teams::Choco},
		{TEXT("/Game/Assets/InkBottle/Materials/Surface/MI_InkSurface_Mint.MI_InkSurface_Mint"), Teams::Mint},
		{TEXT("/Game/Assets/InkBottle/Materials/Surface/MI_InkSurface_Choco.MI_InkSurface_Choco"), Teams::Choco},
		{TEXT("/Game/LevelPrototyping/Paint/Graybox/MI_GB_Mint.MI_GB_Mint"), Teams::Mint},
		{TEXT("/Game/LevelPrototyping/Paint/Graybox/MI_GB_Choco.MI_GB_Choco"), Teams::Choco},
	};
	for (const FTeamInstance& Entry : TeamInstances)
	{
		const UMaterialInstance* const Instance = LoadObject<UMaterialInstance>(nullptr, Entry.Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Entry.Path), Instance))
		{
			continue;
		}
		float TeamId = -1.0f;
		TestTrue(*FString::Printf(TEXT("%s: has TeamId"), Entry.Path), ReadScalar(*Instance, TeamLook::TeamIdParameter, TeamId));
		TestEqual(*FString::Printf(TEXT("%s: TeamId"), Entry.Path), TeamId, static_cast<float>(Entry.Team));
	}

	static const FName UseTeamLookParameter(TEXT("UseTeamLook"));
	for (const TCHAR* const Path : {
			 TEXT("/Game/Assets/InkBottle/Materials/Liquid/MI_InkLiquid_Red.MI_InkLiquid_Red"),
			 TEXT("/Game/Assets/InkBottle/Materials/Surface/MI_InkSurface_Red.MI_InkSurface_Red")})
	{
		const UMaterialInstance* const Instance = LoadObject<UMaterialInstance>(nullptr, Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Path), Instance))
		{
			continue;
		}
		float UseTeamLook = 1.0f;
		ReadScalar(*Instance, UseTeamLookParameter, UseTeamLook);
		TestEqual(*FString::Printf(TEXT("%s: UseTeamLook is off"), Path), UseTeamLook, 0.0f);
	}

	const UClass* const SideSplatClass = UPaintSettings::Get().SideSplatEffectClass.LoadSynchronous();
	if (TestNotNull(TEXT("SideSplatEffectClass loads"), SideSplatClass))
	{
		const APaintSideSplat* const SideSplat = Cast<APaintSideSplat>(SideSplatClass->GetDefaultObject());
		if (TestNotNull(TEXT("SideSplatEffectClass is a PaintSideSplat"), SideSplat))
		{
			TestNotNull(TEXT("side splat has a decal material"), SideSplat->GetDecalMaterial());
		}
	}
	return true;
}

#endif
