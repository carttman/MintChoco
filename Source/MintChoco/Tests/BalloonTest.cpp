#include "Misc/AutomationTest.h"

#include "Game/Balloon.h"
#include "Paint/PaintSplat.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 풍선 규칙: 타격력이 체력에 닿으면 터지고, 마지막으로 때린 팀이 터뜨린 팀이며, 리셋하면 처음으로 돌아간다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBalloonDamageTest,
	"MintChoco.Game.Balloon.Damage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBalloonDamageTest::RunTest(const FString& Parameters)
{
	const float MaxHealth = 100.0f;
	FBalloonState State;

	TestEqual(TEXT("starts empty"), State.GetFraction(MaxHealth), 0.0f);
	TestEqual(TEXT("no team yet"), State.LastTeam, static_cast<int32>(Teams::None));

	// 샷건 알갱이 3짜리 36발: 34발째에 터진다(102 >= 100).
	int32 PoppedAt = 0;
	for (int32 Pellet = 1; Pellet <= 36; ++Pellet)
	{
		if (State.Hit(3.0f, Teams::Mint, MaxHealth))
		{
			PoppedAt = Pellet;
			break;
		}
	}
	TestEqual(TEXT("pops on the 34th pellet of 3"), PoppedAt, 34);
	TestEqual(TEXT("mint popped it"), State.LastTeam, static_cast<int32>(Teams::Mint));
	TestEqual(TEXT("full"), State.GetFraction(MaxHealth), 1.0f);
	TestFalse(TEXT("hits after the pop are ignored"), State.Hit(50.0f, Teams::Choco, MaxHealth));
	TestEqual(TEXT("a late hit does not steal the pop"), State.LastTeam, static_cast<int32>(Teams::Mint));

	// 리셋 뒤: 초코가 99까지 때리고 민트가 마지막 1을 넣으면 민트가 터뜨린 것이다.
	State.Reset();
	TestEqual(TEXT("reset clears damage"), State.Damage, 0.0f);
	TestFalse(TEXT("99 does not pop"), State.Hit(99.0f, Teams::Choco, MaxHealth));
	TestEqual(TEXT("choco is last so far"), State.LastTeam, static_cast<int32>(Teams::Choco));
	TestEqual(TEXT("almost full"), State.GetFraction(MaxHealth), 0.99f, 1e-4f);
	TestTrue(TEXT("the last hit pops"), State.Hit(1.0f, Teams::Mint, MaxHealth));
	TestEqual(TEXT("the last hitter owns the pop"), State.LastTeam, static_cast<int32>(Teams::Mint));

	// 팀이 아닌 페인트 id(예약 id)는 피해만 쌓고 팀은 바꾸지 않는다.
	State.Reset();
	State.Hit(10.0f, Teams::Choco, MaxHealth);
	State.Hit(10.0f, PaintIdNone, MaxHealth);
	TestEqual(TEXT("non-team paint counts as damage"), State.Damage, 20.0f);
	TestEqual(TEXT("non-team paint keeps the last team"), State.LastTeam, static_cast<int32>(Teams::Choco));

	// 스나이퍼 풀충전 100은 한 방.
	State.Reset();
	TestTrue(TEXT("a 100 hit pops at once"), State.Hit(100.0f, Teams::Mint, MaxHealth));
	TestFalse(TEXT("zero power does nothing"), FBalloonState().Hit(0.0f, Teams::Mint, MaxHealth));

	return true;
}

#endif
