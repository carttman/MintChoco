#include "Game/UnitAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

#include "Game/Unit.h"
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

// ---------------------------------------------------------------- UUnitAnimInstance

void UUnitAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Unit = Cast<AUnit>(TryGetPawnOwner());
	bAimPitchInitialized = false;
}

void UUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 블루프린트가 재인스턴스되거나 폰이 늦게 붙는 경우가 있어 매번 다시 본다. 캐스트 하나라 싸다.
	APawn* const Pawn = TryGetPawnOwner();
	Unit = Cast<AUnit>(Pawn);
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
		const UUnitMovementComponent* const UnitMovement = Unit->GetUnitMovement();
		HeroLandingPhase = UnitMovement ? UnitMovement->GetHeroLandingPhase() : EHeroLandingPhase::None;
		bIsFiring = Unit->GetPaintWeapon() && Unit->GetPaintWeapon()->IsTriggerHeld();
	}
	else
	{
		bIsDashing = false;
		bIsStunned = false;
		HeroLandingPhase = EHeroLandingPhase::None;
		bIsFiring = false;
	}
}
