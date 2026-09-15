// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Game/UnitDataAsset.h"
#include "Game/UnitMovementComponent.h"
#include "Unit.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UGameplayEffect;
class UInkBottleComponent;
class UInkTankComponent;
class UItemSlotComponent;
class UMaterialInterface;
class UPaintWeaponComponent;
class USphereComponent;
class UAudioComponent;
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
	virtual void NotifyControllerChanged() override;
	/** 소유 클라이언트에서 빙의가 확정되는 지점(ClientRestart). OnRep_Controller만 믿으면 클라이언트에서 프로브가 안 켜졌다. */
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;

	/**
	 * 다른 플레이어의 카메라가 이 유닛 안에 들어와 있는 동안 메시를 반투명 대체 재질로 바꾼다.
	 * 로컬 연출이라 복제되지 않는다. 카메라를 가진 쪽(CameraProbe)이 겹침 동안만 켠다.
	 */
	void SetCameraFaded(bool bFaded);

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
	 * 몸통이 지금 조준 방향(컨트롤 Yaw)을 봐야 하는지. 무브먼트 컴포넌트가 매 무브마다 묻는다.
	 *
	 * 방아쇠를 당기고 있거나, 충전 중이거나, 마지막 발사 뒤 FaceAimHoldSeconds 안이면 참.
	 * 소유자는 방아쇠 상태를 직접 알고, 서버와 관전 머신은 복제된 충전 상태와 OnFired 시각으로
	 * 같은 답을 낸다. 이동 입력은 여기 없다 — 그쪽은 무브먼트가 가속으로 스스로 안다.
	 */
	bool WantsToFaceAim() const;

	/**
	 * 서버 전용. 아이템 스턴을 건다(UItemSettings::StunDuration, 이어서 SuperArmorDuration).
	 * 슈퍼아머거나 이미 스턴이면 false.
	 */
	bool TryApplyStun();

	/**
	 * 서버 전용. 지속시간을 지정한 스턴. 무기 적중은 짧은 스턴과 짧은 슈퍼아머를 쓴다(FPaintDeposit).
	 * 스턴이 끝나는 순간 SuperArmorSeconds 동안 슈퍼아머가 이어진다. 0 이하의 스턴은 걸지 않는다.
	 */
	bool TryApplyStun(float StunSeconds, float SuperArmorSeconds);

	/**
	 * 이 유닛의 페인트 색. 팀이 있으면 팀 id, 없으면(샘플 맵) 주무기의 페인트 id.
	 * 같은 색의 탄과 효과는 이 유닛을 지나친다.
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	uint8 GetPaintId() const;

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
	 * 대시 중인지. 애님 블루프린트가 이 값으로 보드 상태(시작·루프·끝)를 고르고, 무기가
	 * 방아쇠를 막는 조건으로 쓴다.
	 *
	 * 소유 클라이언트와 서버는 무브먼트 알림(HandleDashStateChanged)에서 바로 쓰고, 나머지
	 * 클라이언트는 서버 값을 복제로 받는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Unit|Dash")
	bool IsDashing() const { return bIsDashing; }

	/**
	 * 보드를 보일지. 애님 인스턴스가 대시 동작(Dash_* 상태)이 실제로 도는 동안 참으로 세운다.
	 * 대시 키가 아니라 동작을 따르므로, 키만 누르고 동작이 시작되지 않는 경우(제자리, 공중)에는
	 * 보드가 나오지 않는다. 카메라 페이드는 UpdateBoardVisibility가 따로 합친다.
	 */
	void SetBoardShown(bool bShown);

	/**
	 * 메시를 캐릭터의 앞 축을 중심으로 굴린다(도). 양수면 오른쪽으로 기운다. 0이면 블루프린트가 놓은
	 * 원래 자세로 돌아간다. 애님 인스턴스가 보드 동작 중 몸이 도는 속도와 이동 속도만큼 매 프레임 넣는다
	 * (UUnitAnimInstance::BoardLean).
	 *
	 * 연출이라 복제하지 않는다. 머신마다 자기가 보는 몸의 요 회전과 속도에서 같은 값을 낸다. 캡슐과 카메라는
	 * 그대로이고, 메시에 붙은 것(보드, 총, 잉크병, 외곽선)만 함께 기운다.
	 */
	void SetMeshLean(float RollDegrees);

	/**
	 * 히어로 랜딩 단계. 애님 블루프린트가 이 값으로 준비·시작 자세를 고른다.
	 *
	 * 단계 기계는 압축 플래그로 굴러가므로 소유자와 서버에만 있다. 다른 클라이언트의 무브먼트는
	 * SimulatedTick만 돌아 단계를 모르므로, 그쪽에는 서버가 복제한 값을 준다(대시와 같은 규칙).
	 */
	UFUNCTION(BlueprintPure, Category = "Unit|HeroLanding")
	EHeroLandingPhase GetHeroLandingPhase() const;

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
	 * 카메라에 붙은 작은 구. 다른 유닛의 캡슐과 겹치는 동안 그 유닛을 반투명하게 만든다.
	 * 로컬 플레이어의 폰에서만 충돌이 켜진다(NotifyControllerChanged). 캡슐과 메시가 Camera
	 * 채널을 무시하므로 붐이 다른 플레이어에게 막히지 않고, 그 대신 이 구가 겹침을 알린다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USphereComponent> CameraProbe;

	/** 카메라가 안에 들어온 유닛의 메시에 씌우는 반투명 재질. 비어 있으면 페이드 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UMaterialInterface> CameraFadeMaterial;

	/**
	 * 슈퍼아머 동안 테두리로 그리는 재질. 비어 있으면 하이라이트가 없다.
	 *
	 * 껍데기 메시(OutlineMesh)의 모든 슬롯에 깔린다. 정점을 법선 방향으로 밀고 앞면을
	 * 잘라내는 재질이어야 테두리로 보인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status")
	TObjectPtr<UMaterialInterface> SuperArmorOutlineMaterial;

	/**
	 * 슈퍼아머 테두리를 그리는 껍데기 메시. 캐릭터 메시와 같은 메시를 리더 포즈로 따라가고,
	 * 머티리얼이 정점을 법선 방향으로 밀어 살짝 부풀린다. 앞면은 머티리얼에서 잘라내므로
	 * 원본 캐릭터에 가려지지 않는 실루엣 바깥쪽만 남는다. 평소에는 꺼 둔다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	TObjectPtr<USkeletalMeshComponent> OutlineMesh;

	/** 조작에 쓰이는 입력 에셋. 비어 있으면 이 유닛은 플레이어 입력을 받지 못한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UUnitInputConfig> InputConfig;

	/**
	 * 마지막 발사 뒤 몸통이 조준 방향을 계속 따르는 시간(초). 애님 인스턴스의 FireHoldTime과
	 * 같은 값이어야 총 든 자세가 내려가는 순간 몸통도 같이 풀린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0", ForceUnits = "s"))
	float FaceAimHoldSeconds = 0.5f;

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

	/**
	 * 손에 든 총. 평소에는 숨어 있고 발사 연출 동안에만 보인다.
	 * 어떤 메시인지와 얼마나 보일지는 UnitData가 정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	/**
	 * 대시 중 발밑의 보드. 평소에는 숨어 있고 대시 **동작**이 시작되면 보이며 끝나면 숨는다.
	 * 스폰하지 않고 켜고 끄므로 복제가 필요 없다: 각 머신의 애님 인스턴스가 자기 화면의 상태
	 * 기계를 보고 SetBoardShown으로 세우므로, 그 머신이 그리는 동작과 항상 일치한다.
	 * 어떤 메시인지는 UnitData가 정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit|Dash")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

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
	/** 대시 의도를 무브먼트 컴포넌트에 전달한다. 컴포넌트 타입이 틀리면 여기서 드러난다. */
	void SetDashInput(bool bWantsToDash);

	/** 실제로 대시 상태가 바뀔 때 무브먼트 컴포넌트가 알려준다. */
	void HandleDashStateChanged(bool bDashing);

	/** 서버 전용. 무브먼트의 단계 변화를 복제 값으로 옮긴다. */
	void HandleHeroLandingPhaseChanged(EHeroLandingPhase NewPhase);

	/** 무기의 페인트 id가 바뀌면(로컬 세팅이든 복제든) 잉크병을 그 팀 색으로 맞춘다. */
	UFUNCTION()
	void HandlePaintIdChanged(uint8 PaintId);

	/**
	 * 어느 무기든 한 발 나갈 때마다. 소유자는 예측 시점, 서버는 실제 발사, 다른 클라이언트는
	 * 샷 멀티캐스트 시점에 온다. 발사 연출(EUnitAction::Fire)을 튼다.
	 */
	UFUNCTION()
	void HandleWeaponFired(int32 Seed);

	/** 연출의 몽타주 부분: 몽타주 에셋이 있으면 그것을, 없으면 Animation을 슬롯에 동적 몽타주로. */
	void PlayFeedbackMontage(const struct FUnitActionFeedback& Feedback);

	/**
	 * 차지샷 충전이 시작·종료될 때. 무기가 알려 준다(UPaintWeaponComponent::OnChargingChanged).
	 *
	 * 충전 중에는 총이 계속 들려 있어야 한다. 총은 캐릭터 메시의 Gun 소켓에 붙어 있어서,
	 * 자세가 내려가면 발사 지점이 쉬는 손으로 돌아간다.
	 */
	UFUNCTION()
	void HandleChargingChanged(bool bCharging);

	/**
	 * 충전 자세를 UpperBody 슬롯에 루프로 건다.
	 *
	 * 애님 그래프가 이 슬롯이 도는지를 보고 상체 자세를 켜므로(Is Slot Active), 그래프에
	 * 따로 배선할 것이 없다 — 슬롯을 채우는 것이 곧 신호다.
	 */
	void StartChargePose();

	/** 걸어 둔 충전 자세만 지목해 세운다. 슬롯째 세우면 방금 시작한 발사 동작까지 끊긴다. */
	void StopChargePose();

	/** 지금 걸려 있는 충전 자세 몽타주. 없으면 비어 있다. */
	TWeakObjectPtr<class UAnimMontage> ChargePose;

	/** 한 발 나갈 때마다. 총을 보이게 하고 유지 시간을 처음부터 다시 센다. */
	void ShowGunForFire();

	/** 유지 시간이 다 됐을 때. */
	void HideGun();

	/** 보임 의도와 카메라 페이드를 합쳐 실제 가시성을 정한다. */
	void UpdateGunVisibility();

	/** 대시 동작이 돌고 있고 카메라 페이드가 아닐 때만 보드가 보인다. */
	void UpdateBoardVisibility();

	/**
	 * 보드 표시에 맞춰 주행 루프를 켜고 끈다. 카메라 페이드는 보지 않는다: 페이드는 그림만
	 * 감추는 것이고 보드는 여전히 달리고 있으므로 소리는 이어져야 한다.
	 */
	void UpdateBoardLoopSound();

	/** 발사 연출이 요구하는 총의 상태. 실제로 보이는지는 카메라 페이드까지 봐야 안다. */
	bool bGunVisible = false;

	/** 애님 인스턴스가 세우는 보드 상태. 실제로 보이는지는 카메라 페이드까지 봐야 안다. */
	bool bBoardShown = false;

	/** 기울이기 전 메시의 기준 회전(캡슐 대비). 처음 기울일 때 한 번 잡는다. */
	FQuat MeshRestRotation = FQuat::Identity;
	bool bMeshRestCaptured = false;

	/** 지금 메시에 걸린 기울기(도). 같은 값이면 트랜스폼을 다시 쓰지 않는다. */
	float MeshLeanDegrees = 0.0f;

	/** 총을 숨기는 타이머. 발사마다 다시 걸려 마지막 한 발에서만 만료된다. */
	FTimerHandle GunHideTimer;

	/** 이 머신의 월드 시계로 잰 마지막 발사 시각. 한 번도 안 쐈으면 음수. */
	double LastFireTime = -1.0;

	UFUNCTION()
	void OnRep_IsDashing();

	/** 대시 트레일을 켜고 끈다. 데디케이티드 서버에서는 아무것도 하지 않는다. */
	void UpdateDashEffects(bool bDashing);

	/** 로컬 플레이어 폰에서만 카메라 프로브의 충돌을 켠다. 꺼질 때는 걸어 둔 페이드를 전부 되돌린다. */
	void UpdateCameraProbe();

	UFUNCTION()
	void OnCameraProbeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCameraProbeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 내 카메라가 반투명하게 만든 유닛들. 프로브가 꺼지거나 내가 사라질 때 되돌린다. */
	TArray<TWeakObjectPtr<AUnit>> CameraFadedUnits;

	/** 페이드 전의 메시 재질. 되돌릴 때 슬롯 순서대로 넣는다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> CameraFadeOriginalMaterials;

	bool bCameraFaded = false;

	/** 스턴 태그가 서고 내릴 때. 서는 순간 방아쇠를 놓는다. */
	void HandleStunTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 서버 전용. 스턴 GE가 제거되면 슈퍼아머를 건다. */
	void HandleStunEnded(const FGameplayEffectRemovalInfo& RemovalInfo);

	/** 진행 중인 스턴이 끝날 때 이어질 슈퍼아머 길이. TryApplyStun이 정한다. */
	float PendingSuperArmorSeconds = 0.0f;

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

	/**
	 * 애니메이션용 히어로 랜딩 단계. 소유자와 서버는 무브먼트에서 직접 읽으므로 쓰지 않는다.
	 * 대시와 같은 이유로 여기 있다: 단계 자체는 압축 플래그라 다른 클라이언트에 닿지 않는다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_HeroLandingPhase)
	EHeroLandingPhase ReplicatedHeroPhase = EHeroLandingPhase::None;

	UFUNCTION()
	void OnRep_HeroLandingPhase();

	/** 지속되는 트레일이라 시작할 때 만들고 끝날 때 직접 꺼야 한다. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> DashTrailComponent;

	/** 보드 주행 루프. 트레일과 같은 이유로 들고 있다가 보드가 사라질 때 직접 멈춘다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BoardAudioComponent;

	/**
	 * 컨텍스트를 넣어준 서브시스템. EndPlay 시점에는 Controller가 이미 떨어져 나갔을
	 * 수 있어 다시 찾아갈 수 없으므로, 넣을 때 기억해 두고 그대로 되돌린다.
	 */
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> AppliedInputSubsystem;

	FDelegateHandle StunTagHandle;

	/** 슈퍼아머 태그가 서고 내릴 때, 모든 머신에서. 태그는 복제되므로 어디서나 같이 보인다. */
	void HandleSuperArmorTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 지금 슈퍼아머인지에 맞춰 테두리 메시를 켜고 끈다. */
	void UpdateSuperArmorOutline();

	FDelegateHandle SuperArmorTagHandle;
};
