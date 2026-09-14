#include "Game/UnitAnimInstance.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

#include "Game/Unit.h"
#include "Items/ItemSlotComponent.h"
#include "MintChoco.h"
#include "Weapons/PaintWeaponComponent.h"

// ---------------------------------------------------------------- FUnitAnimMath

FVector2D FUnitAnimMath::LocalPlanarSpeed(const FVector& Velocity, const FRotator& ActorRotation)
{
	const FVector Planar(Velocity.X, Velocity.Y, 0.0f);
	const FRotator Yaw(0.0f, ActorRotation.Yaw, 0.0f);
	const FVector Local = Yaw.UnrotateVector(Planar);
	return FVector2D(Local.X, Local.Y);
}

float FUnitAnimMath::MoveDirectionDegrees(const FVector& Velocity, const FRotator& ActorRotation)
{
	const FVector2D Local = LocalPlanarSpeed(Velocity, ActorRotation);
	if (Local.IsNearlyZero())
	{
		return 0.0f;
	}
	// atan2(오른쪽, 앞): 앞이 0, 오른쪽이 +90, 왼쪽이 -90, 뒤가 ±180.
	return FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
}

bool FUnitAnimMath::IsFireHoldActive(double Now, double LastFiredTime, float HoldSeconds)
{
	return LastFiredTime >= 0.0 && HoldSeconds > 0.0f && Now - LastFiredTime <= HoldSeconds;
}

// ---------------------------------------------------------------- UUnitAnimInstance

void UUnitAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Unit = Cast<AUnit>(TryGetPawnOwner());
	bAimPitchInitialized = false;

	// 상태 기계는 클래스에 구워져 있어 인스턴스마다 한 번만 찾으면 된다.
	LocomotionMachineIndex = GetStateMachineIndex(LocomotionMachineName);
	UE_CLOG(LocomotionMachineIndex == INDEX_NONE, LogMintChoco, Warning,
		TEXT("%s: 상태 기계 '%s'가 없어 대시 보드가 나오지 않습니다. LocomotionMachineName을 확인하세요."),
		*GetNameSafe(GetClass()), *LocomotionMachineName.ToString());
}

void UUnitAnimInstance::NativeUninitializeAnimation()
{
	// 메시가 애님 인스턴스를 바꾸거나 폰이 사라질 때. 보드는 이 인스턴스가 켠 것이므로 끄고,
	// 무기 델리게이트에 죽은 바인딩을 남기지 않는다.
	if (AUnit* const Bound = BoundUnit.Get())
	{
		Bound->SetBoardShown(false);
	}
	BindWeapons(nullptr);
	Super::NativeUninitializeAnimation();
}

bool UUnitAnimInstance::IsInDashState() const
{
	if (LocomotionMachineIndex == INDEX_NONE)
	{
		return false;
	}
	// 전이가 시작되는 순간 현재 상태가 목표 상태로 바뀌므로, 블렌드 중에도 대시로 친다.
	return GetCurrentStateName(LocomotionMachineIndex).ToString().StartsWith(DashStatePrefix);
}

void UUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 블루프린트가 재인스턴스되거나 폰이 늦게 붙는 경우가 있어 매번 다시 본다. 캐스트 하나라 싸다.
	APawn* const Pawn = TryGetPawnOwner();
	Unit = Cast<AUnit>(Pawn);

	// 발사 알림은 유닛이 바뀐 순간에만 다시 건다. 같은 유닛이면 아무것도 하지 않는다.
	if (Unit.Get() != BoundUnit.Get())
	{
		BindWeapons(Unit);
	}

	const ACharacter* const Character = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Pawn || !Movement)
	{
		return;
	}

	// 이동: 무브먼트의 Velocity. 위치 차분이 아니다(리슨 호스트의 슬로모션 함정).
	const FVector Velocity = Movement->Velocity;
	const FRotator Rotation = Pawn->GetActorRotation();
	const FVector2D Local = FUnitAnimMath::LocalPlanarSpeed(Velocity, Rotation);
	MoveForward = Local.X;
	MoveRight = Local.Y;
	GroundSpeed = Local.Size();
	MoveDirection = FUnitAnimMath::MoveDirectionDegrees(Velocity, Rotation);
	bIsMoving = GroundSpeed > MovingSpeedThreshold;
	bHasMoveInput = Movement->GetCurrentAcceleration().SizeSquared() > FMath::Square(InputAccelerationThreshold);

	// 공중
	bIsInAir = Movement->IsFalling() || Movement->MovementMode == MOVE_Flying || Movement->MovementMode == MOVE_Custom;
	VerticalSpeed = Velocity.Z;

	// 조준: GetBaseAimRotation이 로컬은 컨트롤 회전을, 원격은 복제된 RemoteViewPitch16을 준다.
	// 5.8에서 옛 1바이트 RemoteViewPitch는 리플레이 전용이라 직접 읽으면 원격에서 항상 0이다.
	// 원격 값은 복제 주기로 계단식이라 보간한다. 로컬은 매 프레임 새 값이므로 보간하면 입력 지연만 생긴다.
	const float TargetAimPitch = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
	const bool bSmooth = bAimPitchInitialized && !Pawn->IsLocallyControlled() && AimPitchInterpSpeed > 0.0f;
	AimPitch = bSmooth
		? FMath::FInterpTo(AimPitch, TargetAimPitch, DeltaSeconds, AimPitchInterpSpeed)
		: TargetAimPitch;
	bAimPitchInitialized = true;

	// 상태: 유닛일 때만. 전부 이미 복제되는 값이라 모든 머신에서 같은 포즈가 나온다.
	if (Unit)
	{
		bIsDashing = Unit->IsDashing();
		bIsStunned = Unit->IsStunned();

		// 보드는 키가 아니라 동작을 따른다. 이 머신이 그리는 상태 기계를 보므로 화면과 어긋나지 않는다.
		bDashAnimationActive = IsInDashState();
		Unit->SetBoardShown(bDashAnimationActive);

		// 유닛에게 묻는다: 원격 폰의 단계는 무브먼트가 아니라 복제된 값에서 온다.
		HeroLandingPhase = Unit->GetHeroLandingPhase();
		bIsHeroLanding = HeroLandingPhase != EHeroLandingPhase::None;
		bIsFiring = Unit->GetPaintWeapon() && Unit->GetPaintWeapon()->IsTriggerHeld();

		const UItemSlotComponent* const Slot = Unit->GetItemSlot();
		UAnimSequenceBase* const Pose = Slot ? Slot->GetItemPose() : nullptr;
		bHasItemPose = Pose != nullptr;
		bHasUpperBodyItemPose = bHasItemPose && Slot->GetItemPoseBlend() == EItemPoseBlend::UpperBody;
		bHasFullBodyItemPose = bHasItemPose && !bHasUpperBodyItemPose;

		// 자세가 사라져도 마지막 클립은 그대로 들고 있는다.
		//
		// 가지가 꺼질 때 곧바로 사라지는 것이 아니라 블렌드 아웃되는데, 그동안에도 그 안의
		// 시퀀스 플레이어는 계속 평가된다. 여기를 비우면 클립 없는 플레이어가 레퍼런스 포즈를
		// 내므로 블렌드 시간만큼 T 포즈가 새어 나온다.
		if (Pose)
		{
			ItemPose = Pose;
		}

		const UWorld* const World = GetWorld();
		bRecentlyFired = World && FUnitAnimMath::IsFireHoldActive(World->GetTimeSeconds(), LastFiredTime, FireHoldTime);

		// 무기가 스스로 "자세를 들라"고 말한다. bIsFiring을 읽는 것과 같은 방식이라 복제나
		// 델리게이트가 더 필요 없다: 충전 상태는 이미 복제되므로 구경하는 머신에서도 같다.
		bIsAiming = false;
		for (const UPaintWeaponComponent* const Weapon : { Unit->GetPaintWeapon(), Unit->GetSecondaryWeapon() })
		{
			bIsAiming |= Weapon && Weapon->IsAiming();
		}
	}
	else
	{
		bIsDashing = false;
		bIsStunned = false;
		bDashAnimationActive = false;
		HeroLandingPhase = EHeroLandingPhase::None;
		bIsHeroLanding = false;
		bIsFiring = false;
		bRecentlyFired = false;
		bIsAiming = false;
		// ItemPose는 비우지 않는다. 위와 같은 이유로, 블렌드 아웃되는 동안에도 클립이 있어야 한다.
		bHasItemPose = false;
		bHasFullBodyItemPose = false;
		bHasUpperBodyItemPose = false;
	}

	// 애님 그래프가 볼 값은 이것 하나다. 쏘기 전 · 충전 중 · 쏜 뒤를 모두 합쳐 둔다.
	// 대시(보드) 중에는 방아쇠가 막히므로 쏜 직후의 여운도 보드 자세를 덮지 않는다.
	bWeaponPoseHeld = (bIsAiming || bRecentlyFired) && !bIsDashing;
}

void UUnitAnimInstance::BindWeapons(AUnit* NewUnit)
{
	if (AUnit* const OldUnit = BoundUnit.Get())
	{
		for (UPaintWeaponComponent* const Weapon : { OldUnit->GetPaintWeapon(), OldUnit->GetSecondaryWeapon() })
		{
			if (Weapon)
			{
				Weapon->OnFired.RemoveDynamic(this, &UUnitAnimInstance::HandleWeaponFired);
			}
		}
	}

	BoundUnit = NewUnit;
	// 다른 유닛의 발사 기록을 이어받지 않는다.
	LastFiredTime = -1.0;

	if (!NewUnit)
	{
		return;
	}
	for (UPaintWeaponComponent* const Weapon : { NewUnit->GetPaintWeapon(), NewUnit->GetSecondaryWeapon() })
	{
		if (Weapon)
		{
			Weapon->OnFired.AddUniqueDynamic(this, &UUnitAnimInstance::HandleWeaponFired);
		}
	}
}

void UUnitAnimInstance::HandleWeaponFired(int32 Seed)
{
	// 시각은 이 머신의 월드 시계로 잰다. 알림이 도착한 순간부터 세면 되므로 서버 시각이 필요 없다.
	if (const UWorld* const World = GetWorld())
	{
		LastFiredTime = World->GetTimeSeconds();
	}
}
