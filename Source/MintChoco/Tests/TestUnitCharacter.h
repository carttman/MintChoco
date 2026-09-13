#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

#include "Game/UnitMovementComponent.h"

#include "TestUnitCharacter.generated.h"

/**
 * 조준 방향을 정할 수 있는 최소한의 컨트롤러.
 *
 * APawn::GetControlRotation은 컨트롤러가 없으면 영회전을 돌려준다(가상 함수도 아니다). 조준
 * 트레이스가 그 값을 보므로, 컨트롤러 없는 폰으로는 아래를 볼 수가 없다. AController 자체는
 * abstract이라 스폰되지 않아서 이 껍데기가 필요하다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API ATestUnitController : public AController
{
	GENERATED_BODY()
};

/**
 * 테스트가 단계 기계를 직접 굴릴 수 있게 보호된 진입점만 연다. 동작은 UUnitMovementComponent
 * 그대로다 — 테스트가 보는 것과 게임이 도는 것이 같아야 하기 때문이다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API UTestUnitMovementComponent : public UUnitMovementComponent
{
	GENERATED_BODY()

public:
	using UUnitMovementComponent::PhysCustom;
	using UUnitMovementComponent::UpdateCharacterStateBeforeMovement;
};

/**
 * 히어로 랜딩의 움직임만 보기 위한 최소한의 캐릭터. AUnit은 ASC와 무기와 페인트를 함께
 * 끌고 오므로, 단계 전환만 보는 테스트에는 캡슐과 무브먼트만 있으면 된다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API ATestUnitCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATestUnitCharacter(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer.SetDefaultSubobjectClass<UTestUnitMovementComponent>(ACharacter::CharacterMovementComponentName))
	{
		PrimaryActorTick.bCanEverTick = false;
	}

	UTestUnitMovementComponent* GetTestMovement() const
	{
		return Cast<UTestUnitMovementComponent>(GetCharacterMovement());
	}
};
