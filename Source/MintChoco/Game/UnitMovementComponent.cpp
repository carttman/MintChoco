// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UnitMovementComponent.h"

#include "GameFramework/Character.h"

UUnitMovementComponent::UUnitMovementComponent()
{
	bWantsToDash = 0;
}

// 대쉬 상태라면, 다른 이동 효과를 가진 수치와 혼합한 결과를 반환한다. 현재 기본값은 1.7배
float UUnitMovementComponent::GetMaxSpeed() const
{
	const float BaseSpeed = Super::GetMaxSpeed();

	return bWantsToDash ? BaseSpeed * DashSpeedMultiplier : BaseSpeed;
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

void UUnitMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// 서버가 클라이언트의 의도를 여기서 되살린다. 매 ServerMove마다 호출되므로
	// 값이 실제로 바뀔 때만 알리는 SetWantsToDash를 통한다.
	SetWantsToDash((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
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
}

uint8 FSavedMove_Unit::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsToDash)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

bool FSavedMove_Unit::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	// 대시 상태가 다른 두 무브를 하나로 합치면 대시가 시작되거나 끝난 프레임이
	// 통째로 사라져, 서버가 그 전환을 보지 못한다.
	const FSavedMove_Unit* Other = static_cast<const FSavedMove_Unit*>(NewMove.Get());
	if (Other && bSavedWantsToDash != Other->bSavedWantsToDash)
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
