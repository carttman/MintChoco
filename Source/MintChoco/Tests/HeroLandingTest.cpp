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
 * 착지 경직: 내려선 뒤 LandingRecoverTime 동안은 움직일 수 없고, 그 뒤 스스로 풀린다.
 *
 * 시간을 따로 재지 않고 단계로 다루는 것이 핵심이다. HeroPhaseTime은 저장 무브에 실리므로
 * 보정 후 리플레이에서도 같은 지점에서 풀리고, 서버와 클라이언트가 같은 결론을 낸다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingRecoverTest,
	"MintChoco.Items.HeroLanding.Recover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingRecoverTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("기본 경직은 0.5초다"), FMath::IsNearlyEqual(FHeroLandingParams().LandingRecoverTime, 0.5f));

	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 100.0f));
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Movement))
	{
		return false;
	}

	// 아래를 보고 좌클릭 → 내리꽂기.
	LookAlong(*Movement, FRotator(-45.0f, 0.0f, 0.0f));
	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	if (!TestEqual(TEXT("내리꽂기가 시작된다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive))
	{
		return false;
	}

	// 착지. 곧바로 평소로 돌아가지 않고 경직 단계로 들어간다.
	TestTrue(TEXT("내리꽂기 중의 착지는 히어로 랜딩의 착지다"), Movement->FinishHeroLandingDive());
	TestEqual(TEXT("착지 직후는 경직 단계다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Recover);

	// 이게 이 테스트의 핵심: 경직 중에는 움직일 수 없다.
	TestEqual(TEXT("경직 중에는 최고 속도가 0이다"), Movement->GetMaxSpeed(), 0.0f);
	TestTrue(TEXT("경직 중에는 입력 가속도 먹지 않는다"),
		Movement->ConstrainInputAcceleration(FVector(600.0f, 0.0f, 0.0f)).IsNearlyZero());

	// 시간이 덜 지났으면 아직 풀리지 않는다.
	Movement->UpdateCharacterStateBeforeMovement(0.25f);
	TestEqual(TEXT("0.25초로는 풀리지 않는다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Recover);

	// 다 지나면 스스로 풀린다. 따로 걷어 주는 코드가 없어야 한다.
	Movement->UpdateCharacterStateBeforeMovement(0.25f);
	TestEqual(TEXT("0.5초가 지나면 풀린다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestTrue(TEXT("풀리면 다시 움직일 수 있다"), Movement->GetMaxSpeed() > 0.0f);

	return true;
}

/**
 * 남의 화면에 보이는 남의 캐릭터(시뮬레이션 프록시)는 단계 기계를 돌리지 않아야 한다.
 *
 * 이동 모드는 복제되므로 프록시도 MOVE_Custom에 들어가고, MoveSmooth는 MOVE_Custom이면 속도가
 * 0이어도 PhysCustom을 부른다. 그런데 단계와 이륙점은 압축 플래그를 타므로 프록시에는 없다.
 * 그대로 굴리면 "단계가 없다" 분기가 낙하로 되돌려, 공중에 떠 있어야 할 캐릭터가 프록시
 * 화면에서만 바닥으로 떨어진다. 눈에 띄는 증상인데 호스트 화면에서는 멀쩡해서 늦게 발견됐다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingSimulatedProxyTest,
	"MintChoco.Items.HeroLanding.SimulatedProxy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingSimulatedProxyTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	ATestUnitCharacter* const Character = World->SpawnActor<ATestUnitCharacter>(FVector(0.0f, 0.0f, 800.0f), FRotator::ZeroRotator);
	UTestUnitMovementComponent* const Movement = Character ? Character->GetTestMovement() : nullptr;
	if (!TestNotNull(TEXT("테스트 캐릭터의 무브먼트"), Movement))
	{
		return false;
	}

	// 프록시가 받는 것은 이동 모드뿐이다. 단계는 오지 않으므로 None으로 남는다.
	Character->SetRole(ROLE_SimulatedProxy);
	Movement->SetMovementMode(MOVE_Custom, UUnitMovementComponent::CustomMode_HeroLanding);
	TestEqual(TEXT("프록시에는 단계가 없다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);

	const FVector Before = Character->GetActorLocation();
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Movement->PhysCustom(0.016f, 0);
	}

	// 이게 깨지면 프록시 화면에서만 캐릭터가 공중에서 바닥으로 떨어진다.
	TestEqual(TEXT("프록시는 커스텀 모드에 그대로 있는다"),
		Movement->MovementMode, static_cast<TEnumAsByte<EMovementMode>>(MOVE_Custom));
	TestEqual(TEXT("프록시는 스스로 움직이지 않는다(위치는 복제가 끌고 간다)"),
		Character->GetActorLocation().Z, Before.Z);

	// 복제된 단계는 그대로 받는다. 내리꽂기의 중력을 끄는 판단이 여기 걸려 있다.
	Movement->SetSimulatedHeroLandingPhase(EHeroLandingPhase::Dive);
	TestEqual(TEXT("프록시는 복제된 단계를 받는다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive);
	TestEqual(TEXT("내리꽂기는 프록시에서도 중력이 없다"), Movement->GetGravityZ(), 0.0f);

	// 단계를 직접 굴리는 쪽은 복제 값으로 덮이지 않는다. 지연만큼 어긋나기 때문이다.
	Character->SetRole(ROLE_Authority);
	Movement->SetSimulatedHeroLandingPhase(EHeroLandingPhase::None);
	TestEqual(TEXT("권한 쪽은 복제 값을 무시한다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive);

	return true;
}

/**
 * 착지점은 시선이 맞힌 바닥이거나, 그것이 없으면 시선 방향 사거리 끝(또는 벽 앞)의 바닥이다.
 * 벽·허공·사거리 밖을 봐도 갈 수 있는 끝에 착지점이 잡히고, 그 아래에도 바닥이 없을 때만
 * 착지점이 없어 표시가 감춰지고 좌클릭도 듣지 않는다.
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

	// 벽: 시선이 맞히긴 하지만 설 수 없는 면이다. 벽 바로 앞의 바닥이 착지점이 된다.
	LookAlong(*Movement, FRotator(0.0f, 0.0f, 0.0f));
	if (TestTrue(TEXT("벽을 보면 벽 앞 바닥이 착지점이다"), Movement->ComputeAimTarget(Target)))
	{
		TestTrue(TEXT("벽 앞 착지점은 바닥 높이다"), FMath::IsNearlyEqual(Target.Z, 0.0f, 1.0f));
		TestTrue(TEXT("벽 앞 착지점은 벽 앞에 있다"), Target.X > 0.0f && Target.X < 500.0f);
	}

	// 허공: 시선이 아무것도 맞히지 못하면 시선 방향 사거리 끝의 바닥이 착지점이다.
	LookAlong(*Movement, FRotator(80.0f, 180.0f, 0.0f));
	if (TestTrue(TEXT("하늘을 보면 시선 쪽 바닥이 착지점이다"), Movement->ComputeAimTarget(Target)))
	{
		TestTrue(TEXT("하늘 착지점은 바닥 높이다"), FMath::IsNearlyEqual(Target.Z, 0.0f, 1.0f));
		TestTrue(TEXT("하늘 착지점은 시선의 수평 방향(-X)에 있다"), Target.X < -100.0f);
	}

	// 바닥이긴 하지만 수평 사거리 밖. 사거리 끝으로 잘라 그 바닥에 선다.
	FHeroLandingParams Short = Movement->GetHeroLandingParams();
	Short.MaxAimDistance = 100.0f;
	Movement->SetHeroLandingParams(Short);
	LookAlong(*Movement, FRotator(-60.0f, 180.0f, 0.0f));
	if (TestTrue(TEXT("사거리 밖 바닥을 보면 사거리 끝이 착지점이다"), Movement->ComputeAimTarget(Target)))
	{
		TestTrue(TEXT("잘린 착지점은 바닥 높이다"), FMath::IsNearlyEqual(Target.Z, 0.0f, 1.0f));
		TestTrue(TEXT("잘린 착지점은 사거리 끝에 있다"), FMath::IsNearlyEqual(FVector(Target.X, Target.Y, 0.0f).Size(), 100.0f, 1.0f));
	}

	// 낭떠러지: 사거리 끝 아래에 바닥이 없다. 바닥은 ±4000까지만 있으므로 그 너머를 본다.
	FHeroLandingParams Far = Movement->GetHeroLandingParams();
	Far.MaxAimDistance = 6000.0f;
	Far.AimTraceDistance = 6000.0f;
	Movement->SetHeroLandingParams(Far);
	LookAlong(*Movement, FRotator(0.0f, 180.0f, 0.0f));
	TestFalse(TEXT("사거리 끝 아래에 바닥이 없으면 착지점이 없다"), Movement->ComputeAimTarget(Target));

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

/**
 * 히어로 랜딩으로 공중에 있는 동안에는 밖에서 던져진 속도(점프대)를 받지 않는다.
 *
 * 내리꽂기는 중력을 끈 직선이고, 그 단계를 푸는 유일한 사건이 착지다. 위로 던져지면 중력 0으로
 * 영원히 올라가 착지가 오지 않는다 — 점프대를 착지점으로 고르면 실제로 그랬다. 단계만 걷어내고
 * 던져지게 두는 것으로는 모자란다: 그러면 내리꽂기 → 착지 → 경직이라는 전환이 통째로 사라져,
 * 그 전환을 보고 도는 쪽(애님 블루프린트의 착지 상태)이 나갈 곳을 잃는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingLaunchTest,
	"MintChoco.Items.HeroLanding.Launch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingLaunchTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 400.0f));
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Movement))
	{
		return false;
	}

	LookAlong(*Movement, FRotator(-45.0f, 0.0f, 0.0f));
	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	if (!TestEqual(TEXT("내리꽂기가 시작된다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive))
	{
		return false;
	}
	TestEqual(TEXT("내리꽂는 동안에는 중력이 없다"), Movement->GetGravityZ(), 0.0f);

	// 점프대가 하는 일과 같다(레벨의 발판은 블루프린트지만 Launch Character를 거쳐 여기로 온다).
	const FVector Before = Movement->Velocity;
	Movement->Launch(FVector(0.0f, 0.0f, 1200.0f));

	TestTrue(TEXT("던져진 속도를 쌓아 두지 않는다"), Movement->PendingLaunchVelocity.IsNearlyZero());
	TestEqual(TEXT("내리꽂기는 그대로 이어진다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive);
	TestTrue(TEXT("속도도 그대로다"), Movement->Velocity.Equals(Before));
	TestTrue(TEXT("여전히 내려가는 중이다"), Movement->Velocity.Z < 0.0f);

	// 내려서면 평소대로 경직으로 넘어간다. 이 전환이 사라지지 않는 것이 요점이다.
	TestTrue(TEXT("착지는 그대로 온다"), Movement->FinishHeroLandingDive());
	TestEqual(TEXT("착지 뒤는 경직 단계다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Recover);

	// 경직 중에도 무시한다. 여기가 이 테스트의 핵심이다.
	//
	// 예전에는 받아 주고 경직을 풀었다. 이미 땅에 있으니 던져지는 것 자체는 말이 되고, 경직을
	// 안고 떠오르는 것도 막을 수 있어서였다. 그런데 그러면 Recover가 태어난 그 프레임에 죽는다.
	// 애님 블루프린트의 착지 상태는 그 단계를 보고 들어오고 나가므로(LandingRecoverTime), 한 번도
	// 보이지 않은 단계를 기다리며 갇힌다 — 점프대 위로 내리꽂았을 때 동작이 반복되며 움직일 수
	// 없게 됐다. 아예 던지지 않으면 "잠긴 채로 떠오르는" 일도 "갇히는" 일도 없다.
	Movement->Launch(FVector(0.0f, 0.0f, 1200.0f));
	TestTrue(TEXT("경직 중에도 던져진 속도를 쌓아 두지 않는다"), Movement->PendingLaunchVelocity.IsNearlyZero());
	TestEqual(TEXT("경직 단계가 살아남는다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Recover);

	// 경직은 제 시간을 다 쓰고 스스로 풀린다. 애님이 그 단계를 볼 수 있다는 뜻이다.
	Movement->UpdateCharacterStateBeforeMovement(0.25f);
	TestEqual(TEXT("경직이 발사에 잘려 나가지 않았다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Recover);
	Movement->UpdateCharacterStateBeforeMovement(0.25f);
	TestEqual(TEXT("제 시간을 다 쓰면 풀린다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestTrue(TEXT("풀리면 다시 움직일 수 있다"), Movement->GetMaxSpeed() > 0.0f);

	// 히어로 랜딩이 끝나면 점프대는 평범하게 듣는다. 가드가 그 밖까지 번지면 안 된다.
	Movement->Launch(FVector(0.0f, 0.0f, 1200.0f));
	TestFalse(TEXT("히어로 랜딩이 끝나면 발사가 예약된다"), Movement->PendingLaunchVelocity.IsNearlyZero());

	return true;
}

/**
 * 착지하면 꽂은 쪽을 보고 선다. 내리꽂는 동안 캡슐은 전혀 돌지 않으므로(입력이 잠긴 동안
 * ShouldFaceControlRotation이 거짓) 그 차이는 메시가 들고 있었고, 착지하는 순간 캡슐이 그 각을
 * 이어받는다. 그래서 경직이 풀려 조작이 돌아온 뒤에도 방향이 유지된다.
 *
 * 돌 방향이 없는 착지(바로 발밑으로 떨어지는 수직 낙하)는 몸을 건드리지 않는다. 0도로 돌려버리면
 * 세계의 +X를 보게 되는데, 그것은 "정면"이 아니라 아무 쪽도 아니다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingFacingTest,
	"MintChoco.Items.HeroLanding.LandingFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingFacingTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -100.0f), FVector(4000.0f, 4000.0f, 100.0f));

	// 옆(+Y, 요 90도)을 내려다보고 꽂는다. 몸은 뜰 때 보던 쪽(요 0)을 그대로 보고 있다.
	UTestUnitMovementComponent* const Movement = HoverAt(*World, FVector(0.0f, 0.0f, 100.0f));
	if (!TestNotNull(TEXT("정지 단계의 테스트 캐릭터"), Movement))
	{
		return false;
	}
	ACharacter* const Character = Movement->GetCharacterOwner();

	LookAlong(*Movement, FRotator(-45.0f, 90.0f, 0.0f));
	Movement->SetWantsHeroDive(true);
	Movement->PhysCustom(0.016f, 0);
	if (!TestEqual(TEXT("내리꽂기가 시작된다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::Dive))
	{
		return false;
	}
	TestEqual(TEXT("꽂는 동안에는 몸이 돌지 않는다"), Character->GetActorRotation().Yaw, 0.0, 1e-3);

	TestTrue(TEXT("착지한다"), Movement->FinishHeroLandingDive());
	TestEqual(TEXT("착지하면 꽂은 쪽을 보고 선다"), Character->GetActorRotation().Yaw, 90.0, 1.0);

	// 경직이 풀려도 그대로다. 되돌리는 코드가 없어야 한다.
	Movement->UpdateCharacterStateBeforeMovement(0.5f);
	TestEqual(TEXT("경직이 풀려도 방향은 그대로다"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestEqual(TEXT("조작이 돌아와도 꽂은 쪽을 본다"), Character->GetActorRotation().Yaw, 90.0, 1.0);

	// 바로 발밑으로 떨어지는 착지: 향할 방향이 없으므로 몸을 건드리지 않는다.
	UTestUnitMovementComponent* const Straight = HoverAt(*World, FVector(2000.0f, 0.0f, 100.0f));
	if (!TestNotNull(TEXT("두 번째 테스트 캐릭터"), Straight))
	{
		return false;
	}
	ACharacter* const StraightCharacter = Straight->GetCharacterOwner();
	StraightCharacter->SetActorRotation(FRotator(0.0f, 123.0f, 0.0f));

	LookAlong(*Straight, FRotator(-90.0f, 0.0f, 0.0f));
	Straight->SetWantsHeroDive(true);
	Straight->PhysCustom(0.016f, 0);
	if (!TestEqual(TEXT("수직으로 꽂는다"), Straight->GetHeroLandingPhase(), EHeroLandingPhase::Dive))
	{
		return false;
	}
	TestTrue(TEXT("수직 낙하도 착지한다"), Straight->FinishHeroLandingDive());
	TestEqual(TEXT("돌 방향이 없는 착지는 몸을 건드리지 않는다"),
		StraightCharacter->GetActorRotation().Yaw, 123.0, 1e-3);

	// 히어로 랜딩이 아닌 평범한 착지도 마찬가지다. 단계를 보고 먼저 빠져나간다.
	UTestUnitMovementComponent* const Plain = HoverAt(*World, FVector(-2000.0f, 0.0f, 100.0f));
	if (!TestNotNull(TEXT("세 번째 테스트 캐릭터"), Plain))
	{
		return false;
	}
	ACharacter* const PlainCharacter = Plain->GetCharacterOwner();
	PlainCharacter->SetActorRotation(FRotator(0.0f, -45.0f, 0.0f));
	TestFalse(TEXT("정지 중의 착지는 히어로 랜딩의 착지가 아니다"), Plain->FinishHeroLandingDive());
	TestEqual(TEXT("평범한 착지는 몸을 건드리지 않는다"), PlainCharacter->GetActorRotation().Yaw, -45.0, 1e-3);

	return true;
}

#endif
