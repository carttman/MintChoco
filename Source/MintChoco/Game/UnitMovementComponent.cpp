// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UnitMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Game/Unit.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"

UUnitMovementComponent::UUnitMovementComponent()
{
	bWantsToDash = 0;
	bWantsSpeedBoost = 0;
	bWantsHeroLanding = 0;
	bHeroLandingArmed = 1;
	bWantsHeroDive = 0;

	// 엔진의 컨트롤 회전 추종을 켜 두고 PhysicsRotation에서 게이트로 막는다.
	bUseControllerDesiredRotation = true;
	bOrientRotationToMovement = false;
	// 등각속도라 180도는 0.25초, 90도는 0.125초.
	RotationRate = FRotator(0.0f, 720.0f, 0.0f);
}

void UUnitMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!ShouldFaceControlRotation())
	{
		BoardYawRate = 0.0f;
		return;
	}

	// 보드(대시) 중에는 등각속도 대신 보드 제어기로 돈다. 대시가 끝난 뒤에도 몸이 아직 따라가는 중이면
	// 제어기가 끝까지 데려간다: 그 순간 720도/초로 확 꺾이지 않게.
	const AController* const Controller = CharacterOwner ? CharacterOwner->GetController() : nullptr;
	if (HasValidData() && Controller)
	{
		const double CurrentYaw = UpdatedComponent->GetComponentRotation().Yaw;
		const double TargetYaw = Controller->GetDesiredRotation().Yaw;
		const bool bCatchingUp = BoardYawRate != 0.0f && FMath::Abs(FRotator::NormalizeAxis(TargetYaw - CurrentYaw)) > 1.0;
		if (bWantsToDash || bCatchingUp)
		{
			// 보정 뒤 무브를 재생할 때는 돌리지 않는다. 각속도 상태가 두 번 쌓이고, 회전은 서버가 보정하지
			// 않는 값이라 다음 프레임부터 그대로 이어진다.
			if (CharacterOwner->bClientUpdating)
			{
				return;
			}

			const double NewYaw = MakeBoardTurn().Step(CurrentYaw, TargetYaw, BoardYawRate, DeltaTime);
			MoveUpdatedComponent(FVector::ZeroVector, FRotator(0.0f, NewYaw, 0.0f), /*bSweep=*/false);
			return;
		}
	}

	BoardYawRate = 0.0f;
	Super::PhysicsRotation(DeltaTime);
}

FBoardTurn UUnitMovementComponent::MakeBoardTurn() const
{
	FBoardTurn Turn;
	Turn.MaxYawRate = BoardMaxYawRate;
	Turn.MinYawRate = BoardMinYawRate;
	Turn.ConvergeRate = BoardConvergeRate;
	Turn.AccelInterpSpeed = BoardYawAccelInterpSpeed;
	return Turn;
}

double FBoardTurn::Step(double CurrentYaw, double TargetYaw, float& InOutYawRate, float DeltaTime) const
{
	const float Error = static_cast<float>(FRotator::NormalizeAxis(TargetYaw - CurrentYaw));
	if (DeltaTime <= 0.0f)
	{
		return FRotator::NormalizeAxis(CurrentYaw);
	}

	// 이미 맞춰져 있다. 남은 각속도로 지나쳐 나가지 않게 멈춘다.
	constexpr float ArrivalToleranceDegrees = 0.01f;
	if (FMath::Abs(Error) <= ArrivalToleranceDegrees)
	{
		InOutYawRate = 0.0f;
		return FRotator::NormalizeAxis(TargetYaw);
	}

	// 목표 각속도: 차이에 비례하되 최소와 최대 사이. 최소가 최대보다 크면 최대가 이긴다.
	const float MaxRate = FMath::Max(MaxYawRate, 0.0f);
	const float MinRate = FMath::Clamp(MinYawRate, 0.0f, MaxRate);
	const float Desired = FMath::Sign(Error) * FMath::Clamp(FMath::Abs(Error) * FMath::Max(ConvergeRate, 0.0f), MinRate, MaxRate);

	// 각속도는 목표 각속도로 부드럽게 다가간다. 차이가 생긴 순간 곧바로 최대로 튀지 않는다.
	InOutYawRate = AccelInterpSpeed > 0.0f ? FMath::FInterpTo(InOutYawRate, Desired, DeltaTime, AccelInterpSpeed) : Desired;

	// 같은 방향으로 남은 차이를 넘어서면 목표에 맞춘다. 각속도는 실제로 돈 만큼으로 줄여, 천천히 도는
	// 카메라를 따라갈 때 다음 스텝이 끊기지 않게 한다.
	const float Delta = InOutYawRate * DeltaTime;
	if (Delta * Error > 0.0f && FMath::Abs(Delta) >= FMath::Abs(Error))
	{
		InOutYawRate = Error / DeltaTime;
		return FRotator::NormalizeAxis(TargetYaw);
	}
	return FRotator::NormalizeAxis(CurrentYaw + Delta);
}

bool UUnitMovementComponent::ShouldFaceControlRotation() const
{
	if (IsInputLocked()) return false;

	return !Acceleration.IsNearlyZero() || IsAimHeld();
}

// 스턴·히어로 랜딩이면 0. 아니면 기본 속도에 부스트 배율과 대시 배율을 켜진 만큼 곱한 값.
float UUnitMovementComponent::GetMaxSpeed() const
{
	// 최고 속도 0: CalcVelocity는 MaxSpeed로 나누지 않으므로 안전하고, 제동이 몇 프레임 안에
	// 멈춘다. 공중은 마찰이 0이라 밀려나는 궤적은 그대로 간다. 서버와 클라이언트가 같은 태그를
	// 보므로 리플레이도 일치한다.
	if (IsInputLocked())
	{
		return 0.0f;
	}

	// 부스트와 대시는 둘 다 기본 속도에 곱해진다. 부스트 중에 대시하면 둘을 모두 곱하므로 부스트가
	// 대시보다 느려지는 일이 없다. 두 플래그는 압축 플래그로 서버에 가므로 양쪽이 같은 값을 낸다.
	float Speed = Super::GetMaxSpeed();
	if (bWantsSpeedBoost)
	{
		Speed *= SpeedBoostMultiplier;
	}
	if (bWantsToDash)
	{
		Speed *= DashSpeedMultiplier;
	}
	return Speed;
}

FVector UUnitMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// 서버는 클라이언트가 보낸 가속을 그대로 쓰므로, 입력 핸들러가 아니라 여기서 막아야 권위가 선다.
	if (IsInputLocked())
	{
		return FVector::ZeroVector;
	}
	return Super::ConstrainInputAcceleration(InputAcceleration);
}

void UUnitMovementComponent::SetWantsToDash(bool bNewWantsToDash)
{
	const uint8 NewValue = bNewWantsToDash ? 1 : 0;
	if (bWantsToDash == NewValue)
	{
		return;
	}

	bWantsToDash = NewValue;
	OnDashStateChanged.Broadcast(bNewWantsToDash);
}

void UUnitMovementComponent::SetWantsSpeedBoost(bool bNewWantsSpeedBoost)
{
	const uint8 NewValue = bNewWantsSpeedBoost ? 1 : 0;
	if (bWantsSpeedBoost == NewValue)
	{
		return;
	}

	bWantsSpeedBoost = NewValue;
	OnSpeedBoostStateChanged.Broadcast(bNewWantsSpeedBoost);
}

void UUnitMovementComponent::SetWantsHeroLanding(bool bNewWantsHeroLanding)
{
	bWantsHeroLanding = bNewWantsHeroLanding ? 1 : 0;
}

bool UUnitMovementComponent::IsSpeedBoostAllowed() const
{
	const AUnit* const Unit = Cast<AUnit>(CharacterOwner);
	return !Unit || Unit->IsSpeedBoostAuthorized();
}

bool UUnitMovementComponent::IsHeroLandingAllowed() const
{
	const AUnit* const Unit = Cast<AUnit>(CharacterOwner);
	if (!Unit)
	{
		return true;
	}

	// 효과 태그가 이미 있거나(정상), 그 아이템을 아직 들고 있을 때(활성화 RPC가 무브보다 늦게 오는 창).
	const UAbilitySystemComponent* const AbilitySystem = Unit->GetAbilitySystemComponent();
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_HeroLanding))
	{
		return true;
	}
	const UItemProfile* const Held = Unit->GetItemSlot() ? Unit->GetItemSlot()->GetHeldItem() : nullptr;
	return Held && Held->GetStateTag() == ItemTags::State_Item_HeroLanding;
}

bool UUnitMovementComponent::IsSimulatedProxy() const
{
	return CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
}

void UUnitMovementComponent::SetSimulatedHeroLandingPhase(EHeroLandingPhase NewPhase)
{
	// 단계를 직접 굴리는 쪽은 자기 계산이 진실이다. 복제된 값으로 덮으면 지연만큼 어긋난다.
	if (IsSimulatedProxy())
	{
		HeroPhase = NewPhase;
	}
}

bool UUnitMovementComponent::IsAimHeld() const
{
	const AUnit* const Unit = Cast<AUnit>(CharacterOwner);
	return Unit && Unit->WantsToFaceAim();
}

bool UUnitMovementComponent::IsInputLocked() const
{
	if (HeroPhase != EHeroLandingPhase::None)
	{
		return true;
	}
	const AUnit* const Unit = Cast<AUnit>(CharacterOwner);
	return Unit && Unit->IsMovementInputLocked();
}

void UUnitMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// 서버가 클라이언트의 의도를 여기서 되살린다. 매 ServerMove마다 호출되므로
	// 값이 실제로 바뀔 때만 알리는 SetWantsToDash를 통한다.
	SetWantsToDash((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);

	// 부스트는 아이템이 있어야 한다. 효과 태그가 이미 있거나 그 아이템을 들고 있을 때만
	// 클라이언트의 플래그를 믿는다. 아이템 없이 플래그만 보내는 클라이언트는 기본 속도로 돈다.
	const bool bWantsBoost = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	SetWantsSpeedBoost(bWantsBoost && IsSpeedBoostAllowed());

	const bool bWantsLanding = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	SetWantsHeroLanding(bWantsLanding && IsHeroLandingAllowed());

	// 내리꽂기 요청은 따로 인정 검사를 하지 않는다. 단계가 정지 중일 때만 쓰이고, 그 단계에
	// 들어가는 것 자체가 위의 검사를 이미 통과했다는 뜻이기 때문이다.
	SetWantsHeroDive((Flags & FSavedMove_Character::FLAG_Custom_3) != 0);
}

void UUnitMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// 프록시에도 이 함수는 불린다(SimulateMovement 안에서). 그쪽에는 의도 플래그가 없어 항상
	// 0이므로, 그대로 두면 복제로 받은 단계를 스스로 취소해 낙하로 되돌린다.
	if (IsSimulatedProxy())
	{
		return;
	}

	// 착지 경직. 시간이 다 되면 스스로 풀린다. 이 단계에 있는 동안은 IsInputLocked가 참이라
	// 최고 속도가 0이고 입력 가속도 0이 되므로, 서버와 클라이언트가 같은 결과를 낸다.
	if (HeroPhase == EHeroLandingPhase::Recover)
	{
		HeroPhaseTime += DeltaSeconds;
		if (HeroPhaseTime >= HeroParams.LandingRecoverTime)
		{
			SetHeroPhase(EHeroLandingPhase::None);
			HeroPhaseTime = 0.0f;
		}
	}

	if (bWantsHeroLanding)
	{
		// 0→1 전환에서만 시작한다. 착지 뒤 아직 1인 무브가 몇 개 더 와도 다시 뜨지 않는다.
		if (HeroPhase == EHeroLandingPhase::None && bHeroLandingArmed && MovementMode != MOVE_None)
		{
			StartHeroLanding();
		}
		bHeroLandingArmed = 0;
	}
	else
	{
		bHeroLandingArmed = 1;

		// 어빌리티가 상승·정지 중에 끝났다(상한, 취소). 내리꽂기는 착지까지 그대로 둔다:
		// 클라이언트가 서버보다 먼저 착지해 플래그를 내린 무브가 와도 서버의 착지 효과는 나야 한다.
		if (HeroPhase == EHeroLandingPhase::Rise || HeroPhase == EHeroLandingPhase::Hover)
		{
			AbortHeroLanding();
		}
	}
}

float UUnitMovementComponent::GetHeroCharge() const
{
	// 정지 중이면 지금까지 버틴 양이다. 미리보기가 매 프레임 이 값으로 원을 키운다.
	if (HeroPhase == EHeroLandingPhase::Hover)
	{
		return FMath::Clamp(HeroPhaseTime / FMath::Max(HeroParams.HoverTime, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	}
	// 내리꽂기에 들어섰으면 그때 굳은 값. 착지한 뒤에도 남아 있어 서버가 효과를 낼 때 읽는다.
	return HeroCharge;
}

void UUnitMovementComponent::SetHeroPhase(EHeroLandingPhase NewPhase)
{
	if (HeroPhase == NewPhase)
	{
		return;
	}

	HeroPhase = NewPhase;
	OnHeroLandingPhaseChanged.Broadcast(NewPhase);
}

void UUnitMovementComponent::StartHeroLanding()
{
	SetHeroPhase(EHeroLandingPhase::Rise);
	HeroPhaseTime = 0.0f;
	// 지난 발동에서 남은 요청으로 정지 단계를 건너뛰지 않도록.
	bWantsHeroDive = 0;
	HeroTakeoff = UpdatedComponent->GetComponentLocation();
	HeroDiveTarget = HeroTakeoff;
	HeroCharge = 0.0f;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, CustomMode_HeroLanding);
}

void UUnitMovementComponent::AbortHeroLanding()
{
	SetHeroPhase(EHeroLandingPhase::None);
	HeroPhaseTime = 0.0f;
	bWantsHeroLanding = 0;
	bWantsHeroDive = 0;
	if (MovementMode == MOVE_Custom && CustomMovementMode == CustomMode_HeroLanding)
	{
		SetMovementMode(MOVE_Falling);
	}
}

void UUnitMovementComponent::Launch(const FVector& LaunchVelocity)
{
	switch (HeroPhase)
	{
	case EHeroLandingPhase::Rise:
	case EHeroLandingPhase::Hover:
	case EHeroLandingPhase::Approach:
	case EHeroLandingPhase::Dive:
		// 공중에서는 무시한다. 단계를 걷어내고 던져지게 두면 착지가 오지 않아 단계 전환이
		// 통째로 사라지므로(내리꽂기 → 착지 → 경직), 그 전환을 보는 쪽도 함께 멈춘다.
		return;

	case EHeroLandingPhase::Recover:
		// 내려선 뒤다. 던져지는 것 자체는 말이 되지만, 경직을 안고 가면 입력이 잠긴 채로
		// 떠오른다. 여기서 풀어 주면 그 뒤는 평소의 낙하다.
		AbortHeroLanding();
		break;

	default:
		break;
	}

	Super::Launch(LaunchVelocity);
}

bool UUnitMovementComponent::FinishHeroLandingDive()
{
	if (HeroPhase != EHeroLandingPhase::Dive)
	{
		return false;
	}
	// 곧바로 평소로 돌아가지 않는다. 착지 동작이 도는 동안은 움직일 수 없어야 하는데, 그 판단이
	// 이미 단계에 걸려 있다(IsInputLocked). 시간은 UpdateCharacterStateBeforeMovement가 깎고,
	// HeroPhaseTime은 저장 무브에 실리므로 보정 후 리플레이에서도 같은 지점에서 풀린다.
	const bool bRecovers = HeroParams.LandingRecoverTime > 0.0f;
	SetHeroPhase(bRecovers ? EHeroLandingPhase::Recover : EHeroLandingPhase::None);
	HeroPhaseTime = 0.0f;
	bWantsHeroLanding = 0;
	bWantsHeroDive = 0;
	return true;
}

float UUnitMovementComponent::GetGravityZ() const
{
	// 내리꽂기는 조준한 점을 향한 직선이다. 중력을 그대로 두면 같은 속도로도 궤적이 휘어
	// 표시된 착지점보다 앞에 떨어진다(거리가 멀수록 크게).
	return HeroPhase == EHeroLandingPhase::Dive ? 0.0f : Super::GetGravityZ();
}

void UUnitMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == CustomMode_HeroLanding)
	{
		// 시뮬레이션 프록시도 여기까지 온다: 이동 모드는 복제되고, MoveSmooth는 MOVE_Custom이면
		// 속도가 0이어도 PhysCustom을 부른다(CharacterMovementComponent.cpp). 그런데 그쪽에는
		// 단계도 이륙점도 없으므로, 그대로 굴리면 아래의 "단계가 없다" 분기가 낙하로 되돌려
		// 프록시만 바닥으로 떨어진다. 프록시의 위치는 복제가 끌고 가므로 여기서는 할 일이 없다.
		if (!IsSimulatedProxy())
		{
			PhysHeroLanding(DeltaTime, Iterations);
		}
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

void UUnitMovementComponent::PhysHeroLanding(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !UpdatedComponent)
	{
		return;
	}

	// 단계가 없는데 이 모드에 있다면 무엇인가 어긋난 것이다. 낙하로 돌아간다.
	if (HeroPhase != EHeroLandingPhase::Rise && HeroPhase != EHeroLandingPhase::Hover && HeroPhase != EHeroLandingPhase::Approach)
	{
		AbortHeroLanding();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	HeroPhaseTime += DeltaTime;

	if (HeroPhase == EHeroLandingPhase::Approach)
	{
		PhysHeroApproach(DeltaTime, Iterations);
		return;
	}

	if (HeroPhase == EHeroLandingPhase::Rise)
	{
		// 시간으로 정해지는 곡선(ease-out)이라 프레임률과 무관하고 리플레이에서도 같은 위치가 나온다.
		const float Alpha = FMath::Clamp(HeroPhaseTime / FMath::Max(HeroParams.RiseTime, 0.05f), 0.0f, 1.0f);
		const float Eased = 1.0f - FMath::Square(1.0f - Alpha);
		const FVector Target = HeroTakeoff + FVector(0.0f, 0.0f, HeroParams.RiseHeight * Eased);
		const FVector Delta = Target - UpdatedComponent->GetComponentLocation();

		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), /*bSweep=*/true, Hit);
		Velocity = Delta / DeltaTime;

		// 천장에 막히면 그 자리에서 멈춘다.
		const bool bBlockedAbove = Hit.bBlockingHit && Hit.ImpactNormal.Z < -0.3f;
		if (Alpha >= 1.0f || bBlockedAbove)
		{
			SetHeroPhase(EHeroLandingPhase::Hover);
			HeroPhaseTime = 0.0f;
			Velocity = FVector::ZeroVector;
		}
		return;
	}

	// Hover
	Velocity = FVector::ZeroVector;

	FVector Target;
	const bool bHasTarget = ComputeAimTarget(Target);

	// 좌클릭은 내려설 수 있는 바닥을 보고 있을 때만 듣는다. 벽을 보고 눌렀다면 요청 자체를
	// 버린다: 남겨 두면 나중에 시선이 바닥에 걸리는 순간 뜬금없이 꽂힌다.
	if (bWantsHeroDive && !bHasTarget)
	{
		bWantsHeroDive = 0;
	}

	if (HeroPhaseTime < HeroParams.HoverTime && !bWantsHeroDive)
	{
		return;
	}

	// 버틸수록 세진다. 끝까지 기다리면 최대, 일찍 누르면 그만큼 약하다. StartHeroDive가
	// HeroPhaseTime을 0으로 되돌리므로 그 전에 재 둔다.
	HeroCharge = FMath::Clamp(
		HeroPhaseTime / FMath::Max(HeroParams.HoverTime, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	// 시간이 다 됐는데 착지할 바닥을 못 고른 채다. 로딩 화면처럼 갇히면 안 되므로 제자리에 떨어진다.
	if (!bHasTarget)
	{
		Target = UpdatedComponent->GetComponentLocation() - FVector(0.0f, 0.0f, GetHeroCapsuleHalfHeight());
	}

	StartHeroDive(Target, DeltaTime, Iterations);
}

void UUnitMovementComponent::StartHeroDive(const FVector& Target, float DeltaTime, int32 Iterations)
{
	HeroDiveTarget = Target;
	HeroPhaseTime = 0.0f;
	bWantsHeroDive = 0;

	// 캡슐 중심이 지면의 착지점으로 가면 발이 먼저 땅에 닿아 그만큼 앞에서 멈춘다. 얕게 꽂을수록
	// 그 차이가 커지므로, 캡슐 반높이만큼 올린 점을 향한다. 그러면 발바닥이 착지점에 닿는다.
	const FVector Destination = HeroDiveTarget + FVector(0.0f, 0.0f, GetHeroCapsuleHalfHeight());
	const FVector Location = UpdatedComponent->GetComponentLocation();

	// 착지점이 지금 높이보다 위다(높은 단, 옥상). 위로 향하는 직선은 바닥을 위에서 만나지
	// 못해 그대로 지나쳐 버리므로, 착지점 위까지 건너간 다음 수직으로 떨어뜨린다.
	if (Destination.Z >= Location.Z)
	{
		SetHeroPhase(EHeroLandingPhase::Approach);
		HeroApproachPoint = Destination + FVector(0.0f, 0.0f, HeroParams.DiveApexClearance);
		return;
	}

	FVector DiveDirection = Destination - Location;
	if (!DiveDirection.Normalize())
	{
		DiveDirection = FVector::DownVector;
	}
	SetHeroPhase(EHeroLandingPhase::Dive);
	Velocity = DiveDirection * HeroParams.DiveSpeed;
	SetMovementMode(MOVE_Falling);
	StartNewPhysics(DeltaTime, Iterations);
}

void UUnitMovementComponent::PhysHeroApproach(float DeltaTime, int32 Iterations)
{
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Remaining = HeroApproachPoint - Location;
	float Budget = HeroParams.DiveSpeed * DeltaTime;

	// 높이를 먼저 맞추고 그 다음에 옆으로 간다. 대각선으로 질러가면 올라서려는 단의 모서리에
	// 캡슐이 걸린다: 시선이 윗면을 보고 있다는 것과 몸이 지나갈 자리가 있다는 것은 다르다.
	FVector Delta = FVector::ZeroVector;
	if (Remaining.Z > UE_KINDA_SMALL_NUMBER)
	{
		const float Climb = FMath::Min(Remaining.Z, Budget);
		Delta.Z = Climb;
		Budget -= Climb;
	}

	FVector Flat = Remaining;
	Flat.Z = 0.0f;
	const float FlatDistance = Flat.Size();
	if (Budget > 0.0f && FlatDistance > UE_KINDA_SMALL_NUMBER)
	{
		Delta += Flat / FlatDistance * FMath::Min(FlatDistance, Budget);
	}

	// 착지점 위에 닿았다. 남은 것은 수직 낙하뿐이다.
	if (Delta.IsNearlyZero())
	{
		StartHeroPlunge(DeltaTime, Iterations);
		return;
	}

	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), /*bSweep=*/true, Hit);
	Velocity = Delta / DeltaTime;

	// 길이 막혔다(천장, 시선에는 안 보이던 처마). 더 밀고 가면 벽을 긁으며 헤매게 되므로
	// 그 자리에서 떨어뜨린다. 착지 효과는 떨어진 곳에서 난다.
	if (Hit.bBlockingHit)
	{
		StartHeroPlunge(DeltaTime, Iterations);
	}
}

void UUnitMovementComponent::StartHeroPlunge(float DeltaTime, int32 Iterations)
{
	SetHeroPhase(EHeroLandingPhase::Dive);
	HeroPhaseTime = 0.0f;
	Velocity = FVector::DownVector * HeroParams.DiveSpeed;
	SetMovementMode(MOVE_Falling);
	StartNewPhysics(DeltaTime, Iterations);
}

float UUnitMovementComponent::GetHeroCapsuleHalfHeight() const
{
	return CharacterOwner && CharacterOwner->GetCapsuleComponent()
		? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.0f;
}

bool UUnitMovementComponent::ComputeAimTarget(FVector& OutTarget) const
{
	const UWorld* const World = GetWorld();
	if (!CharacterOwner || !World)
	{
		return false;
	}

	// 눈높이에서 컨트롤 회전 방향으로. 카메라 위치가 아니라 폰 기준이어야 서버가 같은 값을 낸다.
	const FVector Origin = CharacterOwner->GetPawnViewLocation();
	const FVector Direction = CharacterOwner->GetControlRotation().Vector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeroLandingAim), /*bTraceComplex=*/false, CharacterOwner);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * HeroParams.AimTraceDistance, ECC_Visibility, Params);

	// 수평 후보. 걸을 수 있는 바닥을 맞혔으면 그 점, 벽·급경사면 그 바로 앞, 아무것도 없으면
	// 시선의 사거리 끝. 캐릭터가 실제로 설 수 있는지를 묻는 것이므로 판정은 무브먼트의 경사 기준과 같다.
	const bool bDirectFloor = bHit && IsWalkable(Hit);
	FVector Candidate;
	if (bDirectFloor)
	{
		Candidate = Hit.ImpactPoint;
	}
	else if (bHit)
	{
		// 벽 안에 서지 않도록 캡슐 반지름만큼 물러선다.
		const float Radius = CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
		Candidate = Hit.ImpactPoint - Direction * Radius;
	}
	else
	{
		Candidate = Origin + Direction * HeroParams.AimTraceDistance;
	}

	// 이륙점 기준 수평 사거리로 자른다. 갈 수 없는 곳을 보고 있어도 표시는 갈 수 있는 끝에
	// 그려지고, 그곳에 떨어진다. 높이는 자르지 않는다 — 높은 곳도 착지점이다.
	FVector Offset = Candidate - HeroTakeoff;
	Offset.Z = 0.0f;
	const bool bClamped = Offset.SizeSquared() > FMath::Square(HeroParams.MaxAimDistance);
	if (bClamped)
	{
		Offset = Offset.GetSafeNormal() * HeroParams.MaxAimDistance;
	}

	if (bDirectFloor && !bClamped)
	{
		OutTarget = Hit.ImpactPoint;
		return true;
	}

	// 후보 자리 위에서 아래로 바닥을 찾는다. 시작점은 지금 높이나 상승 정점 중 높은 쪽 위라,
	// 정점보다 높은 지붕은 무시되고 그 아래 바닥이 잡힌다.
	const FVector Column = HeroTakeoff + Offset;
	const float Top = FMath::Max(UpdatedComponent->GetComponentLocation().Z, HeroTakeoff.Z + HeroParams.RiseHeight) + GetHeroCapsuleHalfHeight();
	const float Bottom = HeroTakeoff.Z - HeroParams.GroundSearchDepth;
	FHitResult Ground;
	if (!World->LineTraceSingleByChannel(Ground, FVector(Column.X, Column.Y, Top), FVector(Column.X, Column.Y, Bottom), ECC_Visibility, Params)
		|| !IsWalkable(Ground))
	{
		// 그 아래에 설 수 있는 바닥이 없다(낭떠러지, 벽의 윗면이 급경사). 착지점이 없다.
		return false;
	}

	OutTarget = Ground.ImpactPoint;
	return true;
}

FNetworkPredictionData_Client* UUnitMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UUnitMovementComponent* MutableThis = const_cast<UUnitMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Unit(*this);
	}

	return ClientPredictionData;
}

void FSavedMove_Unit::Clear()
{
	Super::Clear();

	bSavedWantsToDash = 0;
	bSavedWantsSpeedBoost = 0;
	bSavedWantsHeroLanding = 0;
	bSavedHeroLandingArmed = 1;
	bSavedWantsHeroDive = 0;
	SavedHeroPhase = EHeroLandingPhase::None;
	SavedHeroPhaseTime = 0.0f;
	SavedHeroTakeoff = FVector::ZeroVector;
	SavedHeroDiveTarget = FVector::ZeroVector;
	SavedHeroApproachPoint = FVector::ZeroVector;
}

uint8 FSavedMove_Unit::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToDash)
	{
		Result |= FLAG_Custom_0;
	}

	if (bSavedWantsSpeedBoost)
	{
		Result |= FLAG_Custom_1;
	}

	if (bSavedWantsHeroLanding)
	{
		Result |= FLAG_Custom_2;
	}

	if (bSavedWantsHeroDive)
	{
		Result |= FLAG_Custom_3;
	}

	return Result;
}

bool FSavedMove_Unit::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	// 대시 상태가 다른 두 무브를 하나로 합치면 대시가 시작되거나 끝난 프레임이
	// 통째로 사라져, 서버가 그 전환을 보지 못한다. 부스트도 같다. 히어로 랜딩은 단계가
	// 시간으로 굴러가므로 단계 중인 무브는 아예 합치지 않는다.
	const FSavedMove_Unit* Other = static_cast<const FSavedMove_Unit*>(NewMove.Get());
	if (Other && (bSavedWantsToDash != Other->bSavedWantsToDash
		|| bSavedWantsSpeedBoost != Other->bSavedWantsSpeedBoost
		|| bSavedWantsHeroLanding != Other->bSavedWantsHeroLanding
		|| bSavedWantsHeroDive != Other->bSavedWantsHeroDive
		|| SavedHeroPhase != EHeroLandingPhase::None
		|| Other->SavedHeroPhase != EHeroLandingPhase::None))
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Unit::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UUnitMovementComponent* Movement = C ? Cast<UUnitMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToDash = Movement->bWantsToDash;
		bSavedWantsSpeedBoost = Movement->bWantsSpeedBoost;
		bSavedWantsHeroLanding = Movement->bWantsHeroLanding;
		bSavedHeroLandingArmed = Movement->bHeroLandingArmed;
		bSavedWantsHeroDive = Movement->bWantsHeroDive;
		SavedHeroCharge = Movement->HeroCharge;
		SavedHeroPhase = Movement->HeroPhase;
		SavedHeroPhaseTime = Movement->HeroPhaseTime;
		SavedHeroTakeoff = Movement->HeroTakeoff;
		SavedHeroDiveTarget = Movement->HeroDiveTarget;
		SavedHeroApproachPoint = Movement->HeroApproachPoint;
	}
}

void FSavedMove_Unit::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	// 보정 후 재생에서 이 무브가 기록해둔 상태로 되돌린다. 재생은 이미 지나간
	// 시간을 다시 계산하는 것이라 연출을 다시 트리거하면 안 되므로, 알림을 내는
	// SetWantsToDash가 아니라 플래그를 직접 되돌린다.
	if (UUnitMovementComponent* Movement = C ? Cast<UUnitMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		Movement->bWantsToDash = bSavedWantsToDash;
		Movement->bWantsSpeedBoost = bSavedWantsSpeedBoost;
		Movement->bWantsHeroLanding = bSavedWantsHeroLanding;
		Movement->bHeroLandingArmed = bSavedHeroLandingArmed;
		Movement->bWantsHeroDive = bSavedWantsHeroDive;
		Movement->HeroCharge = SavedHeroCharge;
		Movement->HeroPhase = SavedHeroPhase;
		Movement->HeroPhaseTime = SavedHeroPhaseTime;
		Movement->HeroTakeoff = SavedHeroTakeoff;
		Movement->HeroDiveTarget = SavedHeroDiveTarget;
		Movement->HeroApproachPoint = SavedHeroApproachPoint;
	}
}

FNetworkPredictionData_Client_Unit::FNetworkPredictionData_Client_Unit(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Unit::AllocateNewMove()
{
	return MakeShared<FSavedMove_Unit>();
}
