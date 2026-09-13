#include "Misc/AutomationTest.h"

#include "Materials/MaterialParameterCollection.h"

#include "Game/TeamLook.h"
#include "Game/TeamTypes.h"
#include "Paint/PaintSettings.h"

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

#endif
