#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

#include "Game/UnitMovementComponent.h"
#include "Tests/TestUnitCharacter.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 60 fps 한 프레임. */
	constexpr float BoardTurnDt = 1.0f / 60.0f;

	FBoardTurn MakeDefaultBoardTurn()
	{
		FBoardTurn Turn;
		Turn.MaxYawRate = 360.0f;
		Turn.MinYawRate = 30.0f;
		Turn.ConvergeRate = 4.0f;
		Turn.AccelInterpSpeed = 10.0f;
		return Turn;
	}

	/** +X를 보고 서 있는 테스트 캐릭터. 컨트롤러가 있어야 컨트롤 회전을 정할 수 있다. */
	UTestUnitMovementComponent* SpawnBoardRider(UWorld& World)
	{
		ATestUnitCharacter* const Character = World.SpawnActor<ATestUnitCharacter>(FVector(0.0f, 0.0f, 500.0f), FRotator::ZeroRotator);
		UTestUnitMovementComponent* const Movement = Character ? Character->GetTestMovement() : nullptr;
		if (!Movement)
		{
			return nullptr;
		}
		if (AController* const Controller = World.SpawnActor<ATestUnitController>())
		{
			Controller->Possess(Character);
		}
		Movement->SetMovementMode(MOVE_Falling);
		return Movement;
	}

	void LookBoardAlong(UTestUnitMovementComponent& Movement, float Yaw)
	{
		if (AController* const Controller = Movement.GetCharacterOwner()->GetController())
		{
			Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		}
	}

	float BoardBodyYaw(const UTestUnitMovementComponent& Movement)
	{
		return static_cast<float>(FRotator::NormalizeAxis(Movement.GetCharacterOwner()->GetActorRotation().Yaw));
	}

	void RideFor(UTestUnitMovementComponent& Movement, float Seconds)
	{
		const int32 Frames = FMath::CeilToInt(Seconds / BoardTurnDt);
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Movement.PhysicsRotation(BoardTurnDt);
		}
	}
}

/**
 * 보드 회전 제어기: 뒤처진 차이가 크면 최대 각속도까지 빨리 올라가고, 가까워질수록 줄어들며,
 * 최소 각속도 덕분에 유한 시간에 정확히 맞춘다. 멈춘 목표를 지나치지 않고, 도는 목표는 일정한
 * 지연으로 따라간다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardTurnControllerTest,
	"MintChoco.Game.Facing.BoardTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardTurnControllerTest::RunTest(const FString& Parameters)
{
	const FBoardTurn Turn = MakeDefaultBoardTurn();

	// 1. 멈춘 목표 179도.
	{
		const double Target = 179.0;
		double Yaw = 0.0;
		float Rate = 0.0f;

		Yaw = Turn.Step(Yaw, Target, Rate, BoardTurnDt);
		TestTrue(TEXT("the first frame starts slowly, not at full speed"), Yaw > 0.0 && Yaw < 720.0 * BoardTurnDt);

		float PeakRate = Rate;
		float SlowestAfterPeak = TNumericLimits<float>::Max();
		float LastRateBeforeArrival = Rate;
		bool bOvershoot = false;
		bool bPastPeak = false;
		int32 ArrivedFrame = INDEX_NONE;
		for (int32 Frame = 1; Frame < 600; ++Frame)
		{
			Yaw = Turn.Step(Yaw, Target, Rate, BoardTurnDt);
			bOvershoot |= Yaw > Target + 1e-3;
			const bool bArrived = FMath::IsNearlyEqual(Yaw, Target, 1e-3);
			if (ArrivedFrame == INDEX_NONE && bArrived)
			{
				ArrivedFrame = Frame;
			}
			if (bArrived)
			{
				continue;
			}
			LastRateBeforeArrival = Rate;
			if (Rate > PeakRate)
			{
				PeakRate = Rate;
			}
			else if (Rate < PeakRate - 1e-3f)
			{
				bPastPeak = true;
			}
			if (bPastPeak)
			{
				SlowestAfterPeak = FMath::Min(SlowestAfterPeak, Rate);
			}
		}

		TestTrue(TEXT("a big gap reaches close to the max rate"), PeakRate > 250.0f);
		TestTrue(TEXT("never faster than the max rate"), PeakRate <= 360.0f + 1e-3f);
		TestTrue(TEXT("slows down as the gap closes"), LastRateBeforeArrival < 60.0f);
		TestTrue(TEXT("never slower than the min rate before arriving"), SlowestAfterPeak >= 30.0f - 1e-3f);
		TestFalse(TEXT("never overshoots a still target"), bOvershoot);
		TestTrue(TEXT("arrives in finite time"), ArrivedFrame != INDEX_NONE && ArrivedFrame < 180);
		TestEqual(TEXT("ends exactly on the target"), Yaw, Target, 1e-3);
		TestEqual(TEXT("the rate is spent after arriving"), Rate, 0.0f);
	}

	// 2. 계속 도는 목표(90도/초): 각속도가 목표와 같아지고, 지연은 90 / ConvergeRate = 22.5도로 일정하다.
	{
		double Yaw = 0.0;
		double Target = 0.0;
		float Rate = 0.0f;
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			Target = FRotator::NormalizeAxis(Target + 90.0 * BoardTurnDt);
			Yaw = Turn.Step(Yaw, Target, Rate, BoardTurnDt);
		}
		TestEqual(TEXT("follows a turning camera at its rate"), Rate, 90.0f, 3.0f);
		TestEqual(TEXT("with a steady lag"), static_cast<float>(FRotator::NormalizeAxis(Target - Yaw)), 22.5f, 2.0f);
	}

	// 3. ±180 경계: 170에서 -170은 오른쪽으로 20도다.
	{
		double Yaw = 170.0;
		float Rate = 0.0f;
		bool bWentLeft = false;
		for (int32 Frame = 0; Frame < 300; ++Frame)
		{
			Yaw = Turn.Step(Yaw, -170.0, Rate, BoardTurnDt);
			bWentLeft |= Rate < 0.0f;
		}
		TestFalse(TEXT("takes the short way across 180"), bWentLeft);
		TestEqual(TEXT("arrives across the seam"), static_cast<float>(FRotator::NormalizeAxis(Yaw + 170.0)), 0.0f, 1e-3f);
	}

	// 4. 왼쪽으로: 각속도가 음수이고 정확히 선다.
	{
		double Yaw = 0.0;
		float Rate = 0.0f;
		Yaw = Turn.Step(Yaw, -90.0, Rate, BoardTurnDt);
		TestTrue(TEXT("turning left has a negative rate"), Rate < 0.0f);
		for (int32 Frame = 0; Frame < 300; ++Frame)
		{
			Yaw = Turn.Step(Yaw, -90.0, Rate, BoardTurnDt);
		}
		TestEqual(TEXT("arrives on the left"), Yaw, -90.0, 1e-3);
	}

	return true;
}

/**
 * 무브먼트: 대시 중에는 보드 제어기로 돌고(평소 720도/초보다 첫 프레임이 느리다), 대시가 끝나도
 * 몸이 따라오는 중이면 제어기가 끝까지 데려가며, 다 따라온 뒤에는 평소 경로로 돌아온다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardTurnMovementTest,
	"MintChoco.Game.Facing.BoardMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardTurnMovementTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	UTestUnitMovementComponent* const Movement = SpawnBoardRider(*World);
	if (!TestNotNull(TEXT("test character"), Movement))
	{
		return false;
	}

	// 보드를 타고 달리는 중에 카메라를 오른쪽 90도로.
	Movement->Acceleration = FVector(600.0f, 0.0f, 0.0f);
	Movement->SetWantsToDash(true);
	LookBoardAlong(*Movement, 90.0f);

	Movement->PhysicsRotation(BoardTurnDt);
	const float FirstYaw = BoardBodyYaw(*Movement);
	TestTrue(TEXT("on the board the first frame turns less than the 720 path"), FirstYaw > 0.0f && FirstYaw < 720.0f * BoardTurnDt - 1.0f);
	TestTrue(TEXT("the board controller holds an angular velocity"), Movement->GetBoardYawRate() > 0.0f);

	RideFor(*Movement, 3.0f);
	TestEqual(TEXT("the board settles exactly on the camera yaw"), BoardBodyYaw(*Movement), 90.0f, 0.01f);

	// 반대로 크게 돌리고 도중에 대시를 놓는다: 720으로 꺾이지 않고 제어기가 끝까지 데려간다.
	LookBoardAlong(*Movement, -90.0f);
	RideFor(*Movement, 0.2f);
	Movement->SetWantsToDash(false);
	const float BeforeRelease = BoardBodyYaw(*Movement);
	Movement->PhysicsRotation(BoardTurnDt);
	const float ReleaseStep = FMath::Abs(static_cast<float>(FRotator::NormalizeAxis(BoardBodyYaw(*Movement) - BeforeRelease)));
	TestTrue(TEXT("leaving the board mid-turn does not snap to the 720 path"), ReleaseStep < 720.0f * BoardTurnDt - 1.0f);

	RideFor(*Movement, 3.0f);
	TestEqual(TEXT("keeps carving after the dash until caught up"), BoardBodyYaw(*Movement), -90.0f, 0.01f);
	TestEqual(TEXT("the board rate is spent once caught up"), Movement->GetBoardYawRate(), 0.0f);

	// 다 따라온 뒤 대시가 아니면 평소 경로: 720도/초로 50 ms에 36도.
	LookBoardAlong(*Movement, 0.0f);
	Movement->PhysicsRotation(0.05f);
	TestEqual(TEXT("off the board the normal 720 path is back"), BoardBodyYaw(*Movement), -54.0f, 0.1f);

	return true;
}

#endif
