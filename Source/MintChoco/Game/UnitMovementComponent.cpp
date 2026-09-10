// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UnitMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Game/Unit.h"
#include "GameFramework/Character.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"

UUnitMovementComponent::UUnitMovementComponent()
{
	bWantsToDash = 0;
	bWantsSpeedBoost = 0;
	bWantsHeroLanding = 0;
	bHeroLandingArmed = 1;
}

// 스턴·히어로 랜딩이면 0, 부스트 중이면 고정 속도, 대시 중이면 기본 속도에 배율을 곱한 값, 아니면 기본 속도.
float UUnitMovementComponent::GetMaxSpeed() const
{
	// 최고 속도 0: CalcVelocity는 MaxSpeed로 나누지 않으므로 안전하고, 제동이 몇 프레임 안에
	// 멈춘다. 공중은 마찰이 0이라 밀려나는 궤적은 그대로 간다. 서버와 클라이언트가 같은 태그를
	// 보므로 리플레이도 일치한다.
	if (IsInputLocked())
	{
		return 0.0f;
	}

	if (bWantsSpeedBoost)
	{
		return SpeedBoostSpeed;
	}

	const float BaseSpeed = Super::GetMaxSpeed();

	return bWantsToDash ? BaseSpeed * DashSpeedMultiplier : BaseSpeed;
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
}

void UUnitMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

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

void UUnitMovementComponent::StartHeroLanding()
{
	HeroPhase = EHeroLandingPhase::Rise;
	HeroPhaseTime = 0.0f;
	HeroTakeoff = UpdatedComponent->GetComponentLocation();
	HeroDiveTarget = HeroTakeoff;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, CustomMode_HeroLanding);
}

void UUnitMovementComponent::AbortHeroLanding()
{
	HeroPhase = EHeroLandingPhase::None;
	HeroPhaseTime = 0.0f;
	bWantsHeroLanding = 0;
	if (MovementMode == MOVE_Custom && CustomMovementMode == CustomMode_HeroLanding)
	{
		SetMovementMode(MOVE_Falling);
	}
}

bool UUnitMovementComponent::FinishHeroLandingDive()
{
	if (HeroPhase != EHeroLandingPhase::Dive)
	{
		return false;
	}
	HeroPhase = EHeroLandingPhase::None;
	HeroPhaseTime = 0.0f;
	bWantsHeroLanding = 0;
	return true;
}

void UUnitMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == CustomMode_HeroLanding)
	{
		PhysHeroLanding(DeltaTime, Iterations);
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
	if (HeroPhase != EHeroLandingPhase::Rise && HeroPhase != EHeroLandingPhase::Hover)
	{
		AbortHeroLanding();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	HeroPhaseTime += DeltaTime;

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
			HeroPhase = EHeroLandingPhase::Hover;
			HeroPhaseTime = 0.0f;
			Velocity = FVector::ZeroVector;
		}
		return;
	}

	// Hover
	Velocity = FVector::ZeroVector;
	if (HeroPhaseTime < HeroParams.HoverTime)
	{
		return;
	}

	// 착지점을 고정하고 내리꽂는다. 낙하 모드라 착지가 ProcessLanded → ACharacter::Landed로 온다.
	HeroDiveTarget = ComputeAimTarget();
	HeroPhase = EHeroLandingPhase::Dive;
	HeroPhaseTime = 0.0f;

	FVector DiveDirection = HeroDiveTarget - UpdatedComponent->GetComponentLocation();
	if (!DiveDirection.Normalize() || DiveDirection.Z > -0.1f)
	{
		DiveDirection = FVector::DownVector;
	}
	Velocity = DiveDirection * HeroParams.DiveSpeed;
	SetMovementMode(MOVE_Falling);
	StartNewPhysics(DeltaTime, Iterations);
}

FVector UUnitMovementComponent::ComputeAimTarget() const
{
	const UWorld* const World = GetWorld();
	if (!CharacterOwner || !World)
	{
		return HeroTakeoff;
	}

	// 눈높이에서 컨트롤 회전 방향으로. 카메라 위치가 아니라 폰 기준이어야 서버가 같은 값을 낸다.
	const FVector Origin = CharacterOwner->GetPawnViewLocation();
	const FVector Direction = CharacterOwner->GetControlRotation().Vector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeroLandingAim), /*bTraceComplex=*/false, CharacterOwner);
	FHitResult Hit;
	FVector Target;
	if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * HeroParams.AimTraceDistance, ECC_Visibility, Params))
	{
		Target = Hit.ImpactPoint;
	}
	else if (Direction.Z < -KINDA_SMALL_NUMBER)
	{
		// 이륙 높이의 수평면과 만나는 점.
		const float Distance = (HeroTakeoff.Z - Origin.Z) / Direction.Z;
		Target = Origin + Direction * FMath::Min(Distance, HeroParams.AimTraceDistance);
	}
	else
	{
		// 위를 보고 있다. 시선의 수평 방향으로 최대 거리.
		FVector Flat = Direction;
		Flat.Z = 0.0f;
		Target = HeroTakeoff + Flat.GetSafeNormal() * HeroParams.MaxAimDistance;
	}

	// 이륙점 기준 수평 거리를 자른다. 높이는 그대로 둔다.
	FVector Offset = Target - HeroTakeoff;
	const float Height = Offset.Z;
	Offset.Z = 0.0f;
	if (Offset.SizeSquared() > FMath::Square(HeroParams.MaxAimDistance))
	{
		Offset = Offset.GetSafeNormal() * HeroParams.MaxAimDistance;
	}
	return HeroTakeoff + Offset + FVector(0.0f, 0.0f, Height);
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
	SavedHeroPhase = EHeroLandingPhase::None;
	SavedHeroPhaseTime = 0.0f;
	SavedHeroTakeoff = FVector::ZeroVector;
	SavedHeroDiveTarget = FVector::ZeroVector;
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
		SavedHeroPhase = Movement->HeroPhase;
		SavedHeroPhaseTime = Movement->HeroPhaseTime;
		SavedHeroTakeoff = Movement->HeroTakeoff;
		SavedHeroDiveTarget = Movement->HeroDiveTarget;
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
		Movement->HeroPhase = SavedHeroPhase;
		Movement->HeroPhaseTime = SavedHeroPhaseTime;
		Movement->HeroTakeoff = SavedHeroTakeoff;
		Movement->HeroDiveTarget = SavedHeroDiveTarget;
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
