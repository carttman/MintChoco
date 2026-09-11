#include "Misc/AutomationTest.h"

#include "Paint/PaintSplat.h"
#include "Paint/PaintStar.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintStarEncodeTest,
	"MintChoco.Paint.Star.Encode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintStarEncodeTest::RunTest(const FString& Parameters)
{
	for (uint8 Id = 0; Id < PaintIdCount; ++Id)
	{
		for (uint8 Gen = 0; Gen <= PaintStarGenMax; ++Gen)
		{
			const uint8 Texel = EncodePaintTexel(Id, Gen);
			const uint8 ExpectedGen = Id == PaintIdNone ? 0 : Gen;
			TestEqual(FString::Printf(TEXT("id round trip %d/%d"), Id, Gen), DecodePaintId(Texel), Id);
			TestEqual(FString::Printf(TEXT("gen round trip %d/%d"), Id, Gen), DecodePaintStarGen(Texel), ExpectedGen);
		}
	}
	TestEqual(TEXT("plain unpainted is the clear byte"), EncodePaintTexel(PaintIdNone, 0), PaintIdNone);
	TestEqual(TEXT("a generation past the maximum clamps"), DecodePaintStarGen(EncodePaintTexel(0, 40)), PaintStarGenMax);
	TestEqual(TEXT("the first star is generation 1"), NextPaintStarGen(0), static_cast<uint8>(1));
	TestEqual(TEXT("the wrap skips 0"), NextPaintStarGen(PaintStarGenMax), static_cast<uint8>(1));

	FPaintLockGens Locks;
	Locks.Gen[1] = 5;
	TestTrue(TEXT("that generation of that team is locked"), Locks.Locks(1, 5));
	TestFalse(TEXT("another generation is not"), Locks.Locks(1, 4));
	TestFalse(TEXT("another team is not"), Locks.Locks(0, 5));
	TestFalse(TEXT("plain paint never locks"), Locks.Locks(0, 0));
	TestFalse(TEXT("reserved ids never lock"), Locks.Locks(5, 5));

	Locks.Gen[3] = 31;
	const FPaintLockGens Unpacked = FPaintLockGens::Unpack(Locks.Pack());
	for (int32 Id = 0; Id < PaintTeamIdCount; ++Id)
	{
		TestEqual(FString::Printf(TEXT("pack round trip %d"), Id), Unpacked.Gen[Id], Locks.Gen[Id]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintStarStateTest,
	"MintChoco.Paint.Star.State",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintStarStateTest::RunTest(const FString& Parameters)
{
	// 첫 스타: 세대 1로 3초 동안 잠기고, 끝나면 그 시각부터 2초 바랜다.
	{
		FStarPaintState State;
		TestFalse(TEXT("nothing to show before the first star"), State.IsVisible(100.0));
		TestEqual(TEXT("the first star is generation 1"), State.Begin(100.0, 3.0f, 2.0f), static_cast<uint8>(1));
		TestTrue(TEXT("active while running"), State.IsActive(101.0));
		TestEqual(TEXT("its generation is locked"), State.GetLockedGen(101.0), static_cast<uint8>(1));
		TestEqual(TEXT("while running the fade would start at the cap"), State.GetFadeStart(), 103.0);
		TestTrue(TEXT("rainbow while running"), State.IsVisible(101.0));

		State.End(102.0);
		TestFalse(TEXT("unlocked once ended"), State.IsActive(102.0));
		TestEqual(TEXT("nothing locked"), State.GetLockedGen(102.0), static_cast<uint8>(0));
		TestEqual(TEXT("the fade starts at the end"), State.GetFadeStart(), 102.0);
		TestTrue(TEXT("still fading"), State.IsVisible(103.9));
		TestFalse(TEXT("faded"), State.IsVisible(104.1));

		// 다 바랜 뒤의 스타는 새 세대: 옛 자국은 보통 페인트로 남는다.
		TestEqual(TEXT("a star after the fade is a new generation"), State.Begin(110.0, 3.0f, 2.0f), static_cast<uint8>(2));
	}

	// 리트리거: 같은 프레임에 End -> Begin. 자국은 세대를 지키고 다시 잠긴다.
	{
		FStarPaintState State;
		State.Begin(0.0, 3.0f, 2.0f);
		State.End(1.0);
		TestEqual(TEXT("a retrigger keeps the generation"), State.Begin(1.0, 3.0f, 2.0f), static_cast<uint8>(1));
		TestTrue(TEXT("locked again"), State.IsActive(1.5));
		TestEqual(TEXT("the cap moved with the new star"), State.GetFadeStart(), 4.0);
	}

	// 바래는 중에 팀원이 스타를 쓰면 자국이 되살아난다; 다 바랜 뒤에는 새 세대다.
	{
		FStarPaintState State;
		State.Begin(0.0, 3.0f, 2.0f);
		State.End(3.0);
		TestEqual(TEXT("a star during the fade takes the trail back"), State.Begin(4.0, 3.0f, 2.0f), static_cast<uint8>(1));
		State.End(5.0);
		TestEqual(TEXT("a star after the fade is new"), State.Begin(7.5, 3.0f, 2.0f), static_cast<uint8>(2));
	}

	// 팀원 둘: 두 번째 스타가 첫 스타 중에 시작하면 같은 세대이고, 마지막 하나가 끝날 때까지 잠긴다.
	{
		FStarPaintState State;
		TestEqual(TEXT("first teammate"), State.Begin(0.0, 3.0f, 2.0f), static_cast<uint8>(1));
		TestEqual(TEXT("second teammate joins the generation"), State.Begin(1.0, 3.0f, 2.0f), static_cast<uint8>(1));
		State.End(3.0);
		TestTrue(TEXT("still locked by the second"), State.IsActive(3.5));
		State.End(3.8);
		TestFalse(TEXT("unlocked after the last"), State.IsActive(3.8));
		TestEqual(TEXT("fade from the last end"), State.GetFadeStart(), 3.8);
	}

	// End가 안 온 스타: 상한이 지나면 잠금이 풀리고, 다음 Begin이 세대를 넘긴다.
	{
		FStarPaintState State;
		State.Begin(0.0, 3.0f, 2.0f);
		TestFalse(TEXT("the cap unlocks a star whose end never came"), State.IsActive(3.5));
		TestEqual(TEXT("the fade starts at the cap"), State.GetFadeStart(), 3.0);
		TestEqual(TEXT("the next star is a new generation"), State.Begin(10.0, 3.0f, 2.0f), static_cast<uint8>(2));
		TestTrue(TEXT("and locks normally"), State.IsActive(11.0));
		State.End(11.0);
		TestFalse(TEXT("its end is not swallowed by the leaked count"), State.IsActive(11.0));
	}

	// 상한 뒤에 늦게 온 End는 바래는 시각을 상한보다 뒤로 미루지 않는다.
	{
		FStarPaintState State;
		State.Begin(0.0, 3.0f, 2.0f);
		State.End(4.0);
		TestEqual(TEXT("a late end fades from the cap"), State.GetFadeStart(), 3.0);
		State.End(4.0);
		TestFalse(TEXT("an extra end is ignored"), State.IsActive(4.0));
		TestEqual(TEXT("and moves nothing"), State.GetFadeStart(), 3.0);
	}

	// 셰이더 상태: 로컬 시계로 옮긴 바래는 시각보다 이른 동안만 잠긴다.
	{
		FPaintStarShaderState Shader;
		Shader.Gen[0] = 4.0f;
		Shader.FadeStart[0] = 50.0f;
		Shader.Gen[1] = 2.0f;
		Shader.FadeStart[1] = 10.0f;
		const FPaintLockGens Locks = Shader.GetLockGens(20.0);
		TestEqual(TEXT("a running star locks its generation"), Locks.Gen[0], static_cast<uint8>(4));
		TestEqual(TEXT("a fading star locks nothing"), Locks.Gen[1], static_cast<uint8>(0));
		TestEqual(TEXT("teams without a star lock nothing"), Locks.Gen[2], static_cast<uint8>(0));
	}
	return true;
}

#endif
