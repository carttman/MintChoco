// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Game/UnitDataAsset.h"
#include "Unit.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UGameplayEffect;
class UInkBottleComponent;
class UInkTankComponent;
class UItemSlotComponent;
class UPaintWeaponComponent;
class UNiagaraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UUnitInputConfig;
class UUnitMovementComponent;
struct FActiveGameplayEffectHandle;
struct FGameplayEffectRemovalInfo;
struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE(FOnHeroLandingFinished);

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
class MINTCHOCO_API AUnit : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 무브먼트 컴포넌트를 UUnitMovementComponent로 바꾸기 위해 ObjectInitializer를 받는다. */
	explicit AUnit(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void Landed(const FHitResult& Hit) override;

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Unit")
	const UUnitDataAsset* GetUnitData() const { return UnitData; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UPaintWeaponComponent* GetPaintWeapon() const { return PaintWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UPaintWeaponComponent* GetSecondaryWeapon() const { return SecondaryWeapon; }

	UFUNCTION(BlueprintPure, Category = "Ink")
	UInkTankComponent* GetInkTank() const { return InkTank; }

	UFUNCTION(BlueprintPure, Category = "Item")
	UItemSlotComponent* GetItemSlot() const { return ItemSlot; }

	UFUNCTION(BlueprintPure, Category = "Ink")
	UInkBottleComponent* GetInkBottle() const { return InkBottle; }

	/** 서버가 클라이언트의 속도 부스트 플래그를 인정해도 되는지. 슬롯 컴포넌트가 답한다. */
	bool IsSpeedBoostAuthorized() const;

	/** PlayerState의 팀. 없으면 Teams::None. 아이템의 페인트 id와 아군 판정이 이 값을 쓴다. */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeam() const;

	/** 스턴 중인지(State.Status.Stunned). 모든 머신에서 답한다. */
	UFUNCTION(BlueprintPure, Category = "Status")
	bool IsStunned() const;

	/** 슈퍼아머 중인지(State.Status.SuperArmor). 스턴과 밀어내기가 먹지 않는다. */
	UFUNCTION(BlueprintPure, Category = "Status")
	bool HasSuperArmor() const;

	/**
	 * 입력으로 움직일 수 없는 상태인지(스턴, 히어로 랜딩). 입력 핸들러와 무브먼트 컴포넌트가
	 * 같은 답을 봐야 서버가 클라이언트의 가속을 그대로 쓰는 경로에서도 권위가 선다.
	 */
	bool IsMovementInputLocked() const;

	/**
	 * 서버 전용. 스턴을 건다(UItemSettings::StunDuration). 슈퍼아머거나 이미 스턴이면 false.
	 * 스턴이 끝나는 순간 슈퍼아머(SuperArmorDuration)가 이어진다.
	 */
	bool TryApplyStun();

	/** 서버 전용. From에서 멀어지는 수평 방향으로 밀어낸다(UItemSettings::Knockback*). 슈퍼아머면 무시. */
	void Knockback(const FVector& From);

	/** 히어로 랜딩의 내리꽂기가 착지한 순간. 서버와 소유 클라이언트에서 온다. 어빌리티가 여기서 끝난다. */
	FOnHeroLandingFinished OnHeroLandingFinished;

	/**
	 * PlayerState의 팀을 무기의 페인트 id로 옮긴다. 잉크병은 그 id를 따라 색이 바뀐다.
	 *
	 * 빙의 시점, 폰의 PlayerState가 복제된 시점, 팀 값 자체가 복제된 시점
	 * (AGamePlayerState::OnRep_Team) 세 곳에서 불린다. 팀은 폰이 아니라 PlayerState에
	 * 실려 오므로 셋의 도착 순서가 보장되지 않는다. 마지막에 무엇이 오든 맞도록
	 * 여러 번 불려도 안전하게 두었다.
	 */
	void ApplyTeamToWeapon();

	// UFUNCTION(BlueprintPure, Category = "Camera")
	// USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
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

	/**
	 * 위아래로 볼 수 있는 한계(도). 수평이 0이고 아래가 음수다.
	 *
	 * 카메라 매니저가 아니라 폰이 들고 있는 이유는 이 값이 곧 무기를 겨눌 수 있는 각도이기
	 * 때문이다. 캐릭터가 바뀌면 사격 각도도 같이 바뀌어야 하고, 카메라 매니저는 리스폰마다
	 * 새로 만들어지므로 값을 둘 자리가 아니다.
	 *
	 * 클램프는 소유 클라이언트의 카메라 매니저가 걸고, 서버는 이미 클램프된 회전을
	 * ServerMove로 받는다. 그래서 서버에 따로 걸 필요가 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "-89.9", ClampMax = "0", ForceUnits = "deg"))
	float ViewPitchMin = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0", ClampMax = "89.9", ForceUnits = "deg"))
	float ViewPitchMax = 60.0f;

	/** 조작에 쓰이는 입력 에셋. 비어 있으면 이 유닛은 플레이어 입력을 받지 못한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUnitInputConfig> InputConfig;

	/**
	 * 주무기. 모든 유닛이 하나씩 들고, 주 발사 입력이 이 방아쇠를 당긴다.
	 *
	 * 무엇을 쏘는지는 컴포넌트의 Profile(무기 프로필 에셋)이 정하고, 이 클래스는
	 * 방아쇠와 팀 색만 넘긴다. 무기 교체는 Profile 교체이지 컴포넌트 교체가 아니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPaintWeaponComponent> PaintWeapon;

	/**
	 * 보조 무기. 보조 발사 입력이 이 방아쇠를 당긴다. Profile이 비어 있으면 아무것도 하지 않는다.
	 *
	 * 두 무기는 한 번에 하나만 쏜다: 한쪽 방아쇠가 당겨진 동안 다른 쪽 입력은 무시된다.
	 * 잉크 탱크는 둘이 같이 쓴다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPaintWeaponComponent> SecondaryWeapon;

	/** 잉크 잔량. 무기가 발사마다 여기서 꺼내 쓰고, 등 뒤 병이 이 값을 보여준다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UInkTankComponent> InkTank;

	/**
	 * 어빌리티 시스템. 아이템 효과(어빌리티, 지속형 GE, 상태 태그)가 여기서 돈다.
	 *
	 * 폰에 두는 이유는 효과가 폰의 것이기 때문이다: 이동 속도, 회전, 무기 잠금은 모두 이
	 * 폰에 걸리고, 폰이 바뀌면 효과도 같이 사라지는 것이 맞다. Mixed 복제: GE는
	 * 소유자에게만, 태그는 모두에게 간다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** 아이템 슬롯. 습득·사용·연출은 전부 이 컴포넌트가 맡고, 유닛은 키만 넘긴다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UItemSlotComponent> ItemSlot;

	/** 등 뒤 잉크병의 액체. 메시의 InkBottle 소켓에 붙고, 출렁임과 잔량 표시를 스스로 돌린다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UInkBottleComponent> InkBottle;

	/** 잉크병 유리. 액체에 딸려 움직일 뿐 로직은 없다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UStaticMeshComponent> InkGlass;

	/** 잉크 수면 디스크. 위치와 기울기는 매 틱 InkBottle이 월드 좌표로 놓는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink")
	TObjectPtr<UStaticMeshComponent> InkSurface;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartFire();
	void StopFire();

	/** 입력이 가로채이거나 매핑이 빠져서 방아쇠가 풀릴 때. 차지형 무기가 이때 발사되면 안 된다. */
	void CancelFire();

	void StartSecondaryFire();
	void StopSecondaryFire();
	void CancelSecondaryFire();

	/**
	 * 홀드형 입력이라 Started와 Completed로 나눠 바인딩한다. Triggered는 눌린 동안
	 * 값 true로 계속 발생할 뿐 뗄 때 false를 내지 않으므로, 하나로 처리하면 해제가
	 * 영영 오지 않는다.
	 */
	void StartDash();
	void StopDash();

	/** 아이템 키. 탭 한 번이 곧 사용이라 Started만 묶는다. */
	void UseItem();

	/** 스턴이 걸리고 풀릴 때, 모든 머신에서. 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Status")
	void BP_OnStunned(bool bStunned);

private:
	/** 어빌리티 액터 정보를 이 폰으로 맞춘다. 서버는 빙의 때, 클라이언트는 PlayerState 도착 때. */
	void InitAbilityActorInfo();

	/** 시야 피치 한계를 소유 클라이언트의 카메라 매니저에 넣는다. 리스폰마다 다시 불러야 한다. */
	void ApplyViewPitchLimits();
	/** 대시 의도를 무브먼트 컴포넌트에 전달한다. 컴포넌트 타입이 틀리면 여기서 드러난다. */
	void SetDashInput(bool bWantsToDash);

	/** 실제로 대시 상태가 바뀔 때 무브먼트 컴포넌트가 알려준다. */
	void HandleDashStateChanged(bool bDashing);

	/** 무기의 페인트 id가 바뀌면(로컬 세팅이든 복제든) 잉크병을 그 팀 색으로 맞춘다. */
	UFUNCTION()
	void HandlePaintIdChanged(uint8 PaintId);

	UFUNCTION()
	void OnRep_IsDashing();

	/** 대시 트레일을 켜고 끈다. 데디케이티드 서버에서는 아무것도 하지 않는다. */
	void UpdateDashEffects(bool bDashing);

	/** 스턴 태그가 서고 내릴 때. 서는 순간 방아쇠를 놓는다. */
	void HandleStunTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 서버 전용. 스턴 GE가 제거되면 슈퍼아머를 건다. */
	void HandleStunEnded(const FGameplayEffectRemovalInfo& RemovalInfo);

	/** 서버 전용. 상태 GE 하나를 SetByCaller 지속시간과 동적 태그로 건다. */
	bool ApplyStatusEffect(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& StatusTag, float Duration, FActiveGameplayEffectHandle& OutHandle);

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

	FDelegateHandle StunTagHandle;
};
