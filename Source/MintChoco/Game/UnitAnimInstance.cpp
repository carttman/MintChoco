#include "Game/UnitAnimInstance.h"

#include "Engine/World.h"
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
}

void UUnitAnimInstance::NativeUninitializeAnimation()
{
	// 메시가 애님 인스턴스를 바꾸거나 폰이 사라질 때. 무기 델리게이트에 죽은 바인딩을 남기지 않는다.
	BindWeapons(nullptr);
	Super::NativeUninitializeAnimation();
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
		const UUnitMovementComponent* const UnitMovement = Unit->GetUnitMovement();
		HeroLandingPhase = UnitMovement ? UnitMovement->GetHeroLandingPhase() : EHeroLandingPhase::None;
		bIsFiring = Unit->GetPaintWeapon() && Unit->GetPaintWeapon()->IsTriggerHeld();

		const UWorld* const World = GetWorld();
		bRecentlyFired = World && FUnitAnimMath::IsFireHoldActive(World->GetTimeSeconds(), LastFiredTime, FireHoldTime);
	}
	else
	{
		bIsDashing = false;
		bIsStunned = false;
		HeroLandingPhase = EHeroLandingPhase::None;
		bIsFiring = false;
		bRecentlyFired = false;
	}
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
