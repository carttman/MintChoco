#include "Misc/AutomationTest.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

#include "Game/UnitMovementComponent.h"
#include "Tests/TestUnitCharacter.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 조준 방향을 바꾼다. 조준 트레이스는 폰이 아니라 컨트롤러의 회전을 본다. */
	void LookAlong(UTestUnitMovementComponent& Movement, const FRotator& Rotation)
	{
		if (AController* const Controller = Movement.GetCharacterOwner()->GetController())
		{
			Controller->SetControlRotation(Rotation);
		}
	}

	/** 상승이 끝나 공중에 멈춘 테스트 캐릭터. 조준과 내리꽂기는 전부 이 단계에서 시작한다. */
	UTestUnitMovementComponent* HoverAt(UWorld& World, const FVector& Takeoff)
	{
		ATestUnitCharacter* const Character = World.SpawnActor<ATestUnitCharacter>(Takeoff, FRotator::ZeroRotator);
		UTestUnitMovementComponent* const Movement = Character ? Character->GetTestMovement() : nullptr;
		if (!Movement)
		{
			return nullptr;
		}

		// 조준은 컨트롤 회전에서 오므로 컨트롤러가 있어야 한다. 빙의가 기본 이동 모드도 넣어 준다:
		// 컨트롤러 없는 폰은 MOVE_None으로 남아 랜딩이 아예 시작되지 않는다.
		if (AController* const Controller = World.SpawnActor<ATestUnitController>())
		{
			Controller->Possess(Character);
		}
		Movement->SetMovementMode(MOVE_Falling);

		FHeroLandingParams Params;
		Params.RiseHeight = 300.0f;
		Params.RiseTime = 0.2f;
		Params.HoverTime = 2.0f;
		Movement->SetHeroLandingParams(Params);

		// 어빌리티가 하는 일과 같다: 의도만 세우면 다음 무브에서 상승이 열린다.
		Movement->SetWantsHeroLanding(true);
		Movement->UpdateCharacterStateBeforeMovement(0.016f);
		Movement->PhysCustom(Params.RiseTime, 0);
		return Movement;
	}
}

/** 히어로 랜딩의 의도 플래그와 단계 상태가 무브먼트 컴포넌트에서 일관되게 다뤄진다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingMovementTest,
	"MintChoco.Items.HeroLanding.Movement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingMovementTest::RunTest(const FString& Parameters)
{
	UUnitMovementComponent* const Movement = NewObject<UUnitMovementComponent>();
	Movement->MovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 600.0f;

	// 기본값: 점프대 높이.
	const FHeroLandingParams& Defaults = Movement->GetHeroLandingParams();
	TestEqual(TEXT("default rise height matches the jump pad apex"), Defaults.RiseHeight, 735.0f);
	TestTrue(TEXT("default hover is two seconds"), FMath::IsNearlyEqual(Defaults.HoverTime, 2.0f));

	FHeroLandingParams Params;
	Params.RiseHeight = 500.0f;
	Params.MaxAimDistance = 1000.0f;
	Movement->SetHeroLandingParams(Params);
	TestEqual(TEXT("params are stored"), Movement->GetHeroLandingParams().MaxAimDistance, 1000.0f);

	// 의도 플래그.
	TestFalse(TEXT("starts without intent"), Movement->WantsHeroLanding());
	TestEqual(TEXT("starts in no phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	Movement->SetWantsHeroLanding(true);
	TestTrue(TEXT("intent is remembered"), Movement->WantsHeroLanding());
	TestEqual(TEXT("intent alone does not start a phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestEqual(TEXT("speed unaffected before the phase starts"), Movement->GetMaxSpeed(), 600.0f);

	// 서버 경로: 소유 유닛이 없으면 인정 검사는 통과한다.
	Movement->SetWantsHeroLanding(false);
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_2);
	TestTrue(TEXT("FLAG_Custom_2 restores the intent"), Movement->WantsHeroLanding());
	TestFalse(TEXT("FLAG_Custom_2 alone leaves boost off"), Movement->WantsSpeedBoost());
	Movement->UpdateFromCompressedFlags(0);
	TestFalse(TEXT("a move without the flag clears the intent"), Movement->WantsHeroLanding());

	// 착지 알림은 내리꽂기 중이 아니면 거짓이다.
	TestFalse(TEXT("a plain landing is not a hero landing"), Movement->FinishHeroLandingDive());

	// 중단은 플래그도 내린다.
	Movement->SetWantsHeroLanding(true);
	Movement->AbortHeroLanding();
	TestFalse(TEXT("abort clears the intent"), Movement->WantsHeroLanding());
	TestEqual(TEXT("abort leaves no phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestEqual(TEXT("abort outside the custom mode keeps the mode"), Movement->MovementMode, static_cast<TEnumAsByte<EMovementMode>>(MOVE_Walking));

	return true;
}

/**
 * 좌클릭(내리꽂기 요청)이 랜딩 의도와 별개의 플래그로 다뤄지는지.
 *
 * 둘은 같은 무브에 함께 실리므로, 한쪽 비트가 다른 쪽을 건드리면 서버가 엉뚱한 단계를 돈다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingDiveFlagTest,
	"MintChoco.Items.HeroLanding.DiveFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingDiveFlagTest::RunTest(const FString& Parameters)
{
	UUnitMovementComponent* const Movement = NewObject<UUnitMovementComponent>();
	Movement->MovementMode = MOVE_Walking;

	TestFalse(TEXT("내리꽂기 요청 없이 시작한다"), Movement->WantsHeroDive());

	// 서버 경로: 두 플래그가 서로를 건드리지 않는다.
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_3);
	TestTrue(TEXT("FLAG_Custom_3가 내리꽂기 요청을 되살린다"), Movement->WantsHeroDive());
	TestFalse(TEXT("내리꽂기 요청만으로는 랜딩 의도가 서지 않는다"), Movement->WantsHeroLanding());

	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_2);
	TestFalse(TEXT("플래그가 빠진 무브는 요청을 지운다"), Movement->WantsHeroDive());
	TestTrue(TEXT("랜딩 의도는 그대로 선다"), Movement->WantsHeroLanding());

	// 중단과 착지는 요청도 함께 걷는다. 남겨 두면 다음 발동이 정지 단계를 건너뛴다.
	Movement->SetWantsHeroDive(true);
	Movement->AbortHeroLanding();
	TestFalse(TEXT("중단은 내리꽂기 요청도 지운다"), Movement->WantsHeroDive());

	return true;
}

/**
 * 이 작업의 핵심: 공중에 멈춰 있을 때 좌클릭이 남은 정지 시간을 기다리지 않는다.
 *
 * 클릭은 플래그 하나를 세울 뿐이고 실제 전환은 무브먼트가 하므로, 여기서 단계 기계를 직접
 * 굴려 확인한다. 플래그가 무시되면 "0.5초 뒤에도 여전히 정지" 이후가 전부 깨진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingDiveOnClickTest,
	"MintChoco.Items.HeroLanding.DiveOnClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingDiveOnClickTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	// 발밑의 넓은 바닥. 윗면이 Z=0이고, 조준 트레이스가 여기를 맞혀야 착지점이 나온다.
	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 100.0f));
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Movement))
	{
		return false;
	}

	// 45도 아래를 본다. 바닥을 맞히므로 착지점이 생긴다.
	LookAlong(*Movement, FRotator(-45.0f, 0.0f, 0.0f));

	Movement->PhysCustom(0.5f, 0);
	TestEqual(TEXT("클릭이 없으면 정지 단계가 이어진다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Hover);

	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	TestEqual(TEXT("좌클릭은 남은 정지 시간을 기다리지 않는다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive);
	TestFalse(TEXT("내리꽂기가 시작되면 요청은 소모된다"), Movement->WantsHeroDive());
	TestTrue(TEXT("내리꽂기는 아래를 향한다"), Movement->Velocity.Z < 0.0f);

	return true;
}

/**
 * 착지점은 "내려설 수 있는 바닥"일 때만 나온다. 벽·허공·사거리 밖은 착지점이 없고, 그러면
 * 착지점 표시도 뜨지 않고 좌클릭도 듣지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingAimTargetTest,
	"MintChoco.Items.HeroLanding.AimTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingAimTargetTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	// 윗면이 Z=0인 바닥과, 캐릭터 정면 500cm에 선 높은 벽.
	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));
	MintChocoTest::SpawnBlock(*World, FVector(520.0f, 0.0f, 1000.0f), FVector(20.0f, 2000.0f, 1000.0f));

	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 100.0f));
	ACharacter* const Character = Movement ? Movement->GetCharacterOwner() : nullptr;
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Character))
	{
		return false;
	}

	FVector Target;

	// 바닥: 착지점이 나오고, 맞힌 바닥 높이에 선다.
	LookAlong(*Movement, FRotator(-60.0f, 0.0f, 0.0f));
	if (TestTrue(TEXT("바닥을 보면 착지점이 나온다"), Movement->ComputeAimTarget(Target)))
	{
		TestTrue(TEXT("착지점은 바닥 위에 있다"), FMath::IsNearlyEqual(Target.Z, 0.0f, 1.0f));
	}

	// 벽: 시선이 맞히긴 하지만 설 수 없는 면이다.
	LookAlong(*Movement, FRotator(0.0f, 0.0f, 0.0f));
	TestFalse(TEXT("벽을 보면 착지점이 없다"), Movement->ComputeAimTarget(Target));

	// 허공: 사거리 안에 아무것도 없다.
	LookAlong(*Movement, FRotator(80.0f, 180.0f, 0.0f));
	TestFalse(TEXT("하늘을 보면 착지점이 없다"), Movement->ComputeAimTarget(Target));

	// 바닥이긴 하지만 수평 사거리 밖. 못 가는 곳에 표시를 그리지 않는다.
	FHeroLandingParams Short = Movement->GetHeroLandingParams();
	Short.MaxAimDistance = 100.0f;
	Movement->SetHeroLandingParams(Short);
	LookAlong(*Movement, FRotator(-60.0f, 180.0f, 0.0f));
	TestFalse(TEXT("사거리 밖 바닥은 착지점이 아니다"), Movement->ComputeAimTarget(Target));

	// 착지점이 없으면 좌클릭 요청은 그 자리에서 버려진다. 남겨 두면 나중에 뜬금없이 꽂힌다.
	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	TestEqual(TEXT("착지점이 없으면 좌클릭은 아무 일도 하지 않는다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Hover);
	TestFalse(TEXT("들어 줄 수 없는 요청은 버린다"), Movement->WantsHeroDive());

	return true;
}

/**
 * 지금 높이보다 위에 있는 바닥도 착지점이 된다. 다만 곧장 꽂을 수는 없다: 위로 향하는 직선은
 * 바닥을 위에서 만나지 못해 그대로 지나쳐 버리므로, 착지점 위로 건너간 뒤 수직으로 떨어진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingHighGroundTest,
	"MintChoco.Items.HeroLanding.HighGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingHighGroundTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 100.0f));
	ACharacter* const Character = Movement ? Movement->GetCharacterOwner() : nullptr;
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Character))
	{
		return false;
	}

	// 눈보다 조금 낮은 단을 앞에 세운다. 윗면은 보이지만, 발바닥을 올려놓을 자리(윗면 + 캡슐
	// 반높이)는 지금 캡슐 중심보다 위다 — 예전 코드가 곧장 아래로 꺾어 버리던 경우다.
	const FVector Eye = Character->GetPawnViewLocation();
	const float LedgeTop = Eye.Z - 20.0f;
	MintChocoTest::SpawnBlock(*World, FVector(500.0f, 0.0f, LedgeTop - 400.0f), FVector(300.0f, 300.0f, 400.0f));

	const FVector LedgeAim(500.0f, 0.0f, LedgeTop);
	LookAlong(*Movement, (LedgeAim - Eye).Rotation());

	FVector Target;
	if (!TestTrue(TEXT("눈높이보다 낮은 단의 윗면은 착지점이다"), Movement->ComputeAimTarget(Target)))
	{
		return false;
	}
	TestTrue(TEXT("착지점은 단 위에 있다"), FMath::IsNearlyEqual(Target.Z, LedgeTop, 2.0f));
	TestTrue(TEXT("착지점이 이륙 지점보다 높다"), Target.Z > 100.0f);

	// 좌클릭: 곧장 꽂는 대신 단 위로 건너간다.
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	if (!TestEqual(TEXT("높은 착지점은 건너가기 단계로 간다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Approach))
	{
		return false;
	}
	// 전환은 정지 단계의 마지막에 일어나므로, 실제로 움직이는 것은 다음 프레임부터다.
	// 높이부터 맞추므로 첫 걸음은 위를 향한다: 대각선으로 질러가면 단의 모서리에 걸린다.
	Movement->PhysCustom(0.016f, 0);
	TestTrue(TEXT("건너가기는 높이부터 맞춘다"), Movement->Velocity.Z > 0.0f);
	TestTrue(TEXT("높이를 맞추는 동안에는 옆으로 가지 않는다"), Movement->Velocity.Size2D() < 1.0f);

	// 충분히 굴리면 착지점 위에 닿고 수직 낙하로 넘어간다.
	for (int32 Step = 0; Step < 60 && Movement->GetHeroLandingPhase() == EHeroLandingPhase::Approach; ++Step)
	{
		Movement->PhysCustom(0.016f, 0);
	}
	TestEqual(TEXT("건너간 뒤에는 수직 낙하다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive);

	// 수직 낙하는 착지점 바로 위에서 시작한다. 옆에서 들어가면 단의 벽면에 걸린다.
	const FVector Location = Character->GetActorLocation();
	TestTrue(TEXT("낙하는 착지점 바로 위에서 시작한다"),
		FVector::DistXY(Location, Target) < 20.0f);
	TestTrue(TEXT("낙하는 착지점보다 위에서 시작한다"), Location.Z > Target.Z + HalfHeight);

	return true;
}

#endif
