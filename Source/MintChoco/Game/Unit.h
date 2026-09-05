// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/UnitDataAsset.h"
#include "Unit.generated.h"

class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class USpringArmComponent;
class UUnitInputConfig;
struct FInputActionValue;

/**
 * 플레이어와 AI가 함께 쓰는 유일한 유닛 클래스.
 *
 * 캐릭터 종류는 이 클래스를 상속해서 만들지 않는다. 민트와 초코는 이동, 이동 가속,
 * 페인트 총이 전부 같은 코드로 돌고 애니메이션과 이펙트만 다르므로, 그 차이는
 * UnitData 한 곳에 모여 있다. 캐릭터가 늘어도 이 클래스는 그대로다.
 *
 * 그래서 이 클래스 안에는 "지금 민트인가?"를 묻는 분기가 있어서는 안 된다.
 * 그런 분기가 하나라도 생겼다면 그 값이 UnitData로 가야 한다는 뜻이다.
 * 모든 연출은 예외 없이 PlayActionFeedback()을 통과한다.
 */
UCLASS()
class MINTCHOCO_API AUnit : public ACharacter
{
	GENERATED_BODY()

public:
	AUnit();

	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Unit")
	const UUnitDataAsset* GetUnitData() const { return UnitData; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/**
	 * 서버 전용. 런타임에 캐릭터를 교체한다.
	 *
	 * 지금은 블루프린트 기본값으로 정해지지만, 캐릭터 선택이 로비로 올라가면
	 * 게임모드가 스폰 직후 이 함수를 부르는 것으로 바뀐다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unit")
	void SetUnitData(UUnitDataAsset* NewUnitData);

	/**
	 * 동작 하나의 연출(몽타주 + 이펙트 + 사운드)을 한 번에 재생한다.
	 * 캐릭터별 차이가 드러나는 유일한 지점이며, 호출부는 어느 캐릭터인지 알 필요가 없다.
	 *
	 * 판정이 아니라 연출이므로 어느 머신에서 돌릴지는 호출하는 쪽이 정한다.
	 * 보통 로컬에서 즉시 한 번, 나머지 클라이언트는 멀티캐스트로 한 번이다.
	 *
	 * @return 재생된 몽타주의 길이. 몽타주가 없으면 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unit|Feedback")
	float PlayActionFeedback(EUnitAction Action);

	/** 소켓이 지정되지 않은 이펙트를 지정한 위치에 띄운다. 피격 지점 등에 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "Unit|Feedback")
	float PlayActionFeedbackAtLocation(EUnitAction Action, const FVector& WorldLocation);

protected:
	/**
	 * 이 유닛이 어떤 캐릭터인지.
	 *
	 * 대개는 블루프린트 기본값으로 결정되고, 그 경우 값이 아키타입과 같으므로
	 * 복제 트래픽이 발생하지 않는다. 런타임에 바뀔 때만 실제로 전송된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_UnitData, Category = "Unit")
	TObjectPtr<UUnitDataAsset> UnitData;

	UFUNCTION()
	void OnRep_UnitData();

	/** 메시와 애님 클래스를 UnitData에 맞춘다. 서버와 클라이언트 양쪽에서 돈다. */
	void ApplyUnitData();

	/** 카메라 붐. 컨트롤 회전을 그대로 따라가므로 캐릭터의 회전과 무관하게 돈다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 조작에 쓰이는 입력 에셋. 비어 있으면 이 유닛은 플레이어 입력을 받지 못한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUnitInputConfig> InputConfig;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

private:
	/**
	 * 컨텍스트를 넣어준 서브시스템. EndPlay 시점에는 Controller가 이미 떨어져 나갔을
	 * 수 있어 다시 찾아갈 수 없으므로, 넣을 때 기억해 두고 그대로 되돌린다.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedInputSubsystem;
};
