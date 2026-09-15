#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"

#include "Game/UnitMovementComponent.h"
#include "Tests/TestUnitCharacter.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** +X를 보고 서 있는 테스트 캐릭터. 컨트롤러가 있어야 컨트롤 회전이 영이 아니다. */
	UTestUnitMovementComponent* SpawnFacingX(UWorld& World)
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
		// 바닥이 없으므로 낙하 모드. 회전은 이동 모드와 무관하게 PhysicsRotation이 돌린다.
		Movement->SetMovementMode(MOVE_Falling);
		return Movement;
	}

	void LookAlong(UTestUnitMovementComponent& Movement, float Yaw)
	{
		if (AController* const Controller = Movement.GetCharacterOwner()->GetController())
		{
			Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		}
	}

	float BodyYaw(const UTestUnitMovementComponent& Movement)
	{
		return static_cast<float>(FRotator::NormalizeAxis(Movement.GetCharacterOwner()->GetActorRotation().Yaw));
	}
}

/** 둘러보기만 할 때는 몸통이 그대로고, 이동 가속이 있을 때만 RotationRate로 컨트롤 요를 향해 돈다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitFacingIdleAndMoveTest,
	"MintChoco.Game.Facing.IdleAndMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnitFacingIdleAndMoveTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	UTestUnitMovementComponent* const Movement = World ? SpawnFacingX(*World) : nullptr;
	if (!TestNotNull(TEXT("test character"), Movement))
	{
		MintChocoTest::DestroyWorld(World);
		return false;
	}

	TestTrue(TEXT("engine controller-rotation path is armed"), Movement->bUseControllerDesiredRotation);
	TestFalse(TEXT("never orients to movement"), Movement->bOrientRotationToMovement);
	TestEqual(TEXT("720 deg/s: 180 degrees in a quarter second"), Movement->RotationRate.Yaw, 720.0);

	// 둘러보기: 컨트롤 요만 돌고 몸통은 그대로.
	LookAlong(*Movement, 90.0f);
	Movement->Acceleration = FVector::ZeroVector;
	TestFalse(TEXT("idle look-around does not ask for a turn"), Movement->ShouldFaceControlRotation());
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("idle body keeps its yaw while the camera orbits"), BodyYaw(*Movement), 0.0f, 1e-3f);

	// 이동 입력: 등각속도로 컨트롤 요를 향해 돈다.
	Movement->Acceleration = FVector(600.0f, 0.0f, 0.0f);
	TestTrue(TEXT("movement input asks for a turn"), Movement->ShouldFaceControlRotation());
	Movement->PhysicsRotation(0.05f);
	TestEqual(TEXT("720 deg/s turns 36 degrees in 50 ms"), BodyYaw(*Movement), 36.0f, 0.1f);
	Movement->PhysicsRotation(1.0f);
	TestEqual(TEXT("the turn stops exactly on the control yaw"), BodyYaw(*Movement), 90.0f, 0.1f);

	// 다시 멈추면 카메라를 돌려도 그 자리.
	Movement->Acceleration = FVector::ZeroVector;
	LookAlong(*Movement, -90.0f);
	Movement->PhysicsRotation(1.0f);
	TestEqual(TEXT("stopping frees the body from the camera again"), BodyYaw(*Movement), 90.0f, 0.1f);

	MintChocoTest::DestroyWorld(World);
	return true;
}

/** 히어로 랜딩 단계(입력 잠금)에서는 이동 가속이 있어도 몸통이 돌지 않는다. 유닛이 아니면 조준 게이트는 없다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitFacingLockedTest,
	"MintChoco.Game.Facing.Locked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnitFacingLockedTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	UTestUnitMovementComponent* const Movement = World ? SpawnFacingX(*World) : nullptr;
	if (!TestNotNull(TEXT("test character"), Movement))
	{
		MintChocoTest::DestroyWorld(World);
		return false;
	}

	LookAlong(*Movement, 90.0f);

	// 유닛이 아닌 캐릭터에는 무기가 없으므로 조준만으로는 돌 이유가 없다.
	Movement->Acceleration = FVector::ZeroVector;
	TestFalse(TEXT("no weapon, no aim gate"), Movement->ShouldFaceControlRotation());

	// 히어로 랜딩 단계에 들어가면 입력이 잠긴다.
	FHeroLandingParams Params;
	Params.RiseHeight = 300.0f;
	Params.RiseTime = 0.2f;
	Params.HoverTime = 2.0f;
	Movement->SetHeroLandingParams(Params);
	Movement->SetWantsHeroLanding(true);
	Movement->UpdateCharacterStateBeforeMovement(0.016f);
	Movement->PhysCustom(Params.RiseTime, 0);
	TestNotEqual(TEXT("the phase machine started"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);

	Movement->Acceleration = FVector(600.0f, 0.0f, 0.0f);
	TestFalse(TEXT("locked input never asks for a turn"), Movement->ShouldFaceControlRotation());
	Movement->PhysicsRotation(1.0f);
	TestEqual(TEXT("locked body keeps its yaw"), BodyYaw(*Movement), 0.0f, 1e-3f);

	MintChocoTest::DestroyWorld(World);
	return true;
}

#endif
