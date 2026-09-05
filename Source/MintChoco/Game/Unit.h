// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/UnitDataAsset.h"
#include "Unit.generated.h"

class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UNiagaraComponent;
class USpringArmComponent;
class UUnitInputConfig;
class UUnitMovementComponent;
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
 * 연출은 UnitData의 ActionFeedback에서 꺼내 쓰며, 어느 캐릭터인지 묻지 않는다.
 */
UCLASS()
class MINTCHOCO_API AUnit : public ACharacter
{
	GENERATED_BODY()

public:
	/** 무브먼트 컴포넌트를 UUnitMovementComponent로 바꾸기 위해 ObjectInitializer를 받는다. */
	explicit AUnit(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Unit")
	const UUnitDataAsset* GetUnitData() const { return UnitData; }

	/**
	 * 캐싱하지 않고 매번 GetCharacterMovement()에서 구한다.
	 *
	 * 생성자에서 캐싱하면, 블루프린트가 상속받은 무브먼트 컴포넌트를 자기 템플릿으로
	 * 다시 인스턴스화할 때 캐시가 버려진 컴포넌트를 가리킨 채 남는다. 그 상태에서
	 * 대시 플래그를 세우면 실제로 캐릭터를 움직이는 컴포넌트가 아닌 쪽에 쓰이므로,
	 * 크래시 없이 속도만 그대로인 증상이 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "Unit")
	UUnitMovementComponent* GetUnitMovement() const;

	/**
	 * 대시 중인지. 애님 블루프린트가 이 값으로 스프린트 상태를 고른다.
	 *
	 * 소유 클라이언트는 예측된 값을 즉시 보고, 나머지 클라이언트는 복제로 받는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Unit|Dash")
	bool IsDashing() const { return bIsDashing; }

	/**
	 * 서버 전용. 런타임에 캐릭터를 교체한다.
	 *
	 * 지금은 블루프린트 기본값으로 정해지지만, 캐릭터 선택이 로비로 올라가면
	 * 게임모드가 스폰 직후 이 함수를 부르는 것으로 바뀐다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Unit")
	void SetUnitData(UUnitDataAsset* NewUnitData);

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

	/**
	 * 홀드형 입력이라 Started와 Completed로 나눠 바인딩한다. Triggered는 눌린 동안
	 * 값 true로 계속 발생할 뿐 뗄 때 false를 내지 않으므로, 하나로 처리하면 해제가
	 * 영영 오지 않는다.
	 */
	void StartDash();
	void StopDash();

private:
	/** 대시 의도를 무브먼트 컴포넌트에 전달한다. 컴포넌트 타입이 틀리면 여기서 드러난다. */
	void SetDashInput(bool bWantsToDash);

	/** 실제로 대시 상태가 바뀔 때 무브먼트 컴포넌트가 알려준다. */
	void HandleDashStateChanged(bool bDashing);

	UFUNCTION()
	void OnRep_IsDashing();

	/** 대시 트레일을 켜고 끈다. 데디케이티드 서버에서는 아무것도 하지 않는다. */
	void UpdateDashEffects(bool bDashing);

	/**
	 * 연출과 애니메이션용 대시 상태.
	 *
	 * 대시 의도 자체는 압축 플래그로 서버까지만 가고 다른 클라이언트에는 닿지 않는다.
	 * 그래서 서버가 이 값을 복제해 준다. 소유자는 이미 예측으로 알고 있으므로 제외한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsDashing)
	bool bIsDashing = false;

	/** 지속되는 트레일이라 시작할 때 만들고 끝날 때 직접 꺼야 한다. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> DashTrailComponent;

	/**
	 * 컨텍스트를 넣어준 서브시스템. EndPlay 시점에는 Controller가 이미 떨어져 나갔을
	 * 수 있어 다시 찾아갈 수 없으므로, 넣을 때 기억해 두고 그대로 되돌린다.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedInputSubsystem;
};
