// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnitMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDashStateChanged, bool /*bDashing*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpeedBoostStateChanged, bool /*bBoosting*/);

/** 히어로 랜딩의 단계. None이 아니면 입력이 막히고 무브먼트가 캐릭터를 끌고 간다. */
UENUM(BlueprintType)
enum class EHeroLandingPhase : uint8
{
	None,
	/** 이륙점에서 RiseHeight까지 올라간다. */
	Rise,
	/** 공중에 멈춰 착지점을 고른다. */
	Hover,
	/** 고정된 착지점으로 내리꽂힌다(MOVE_Falling). 착지하면 끝. */
	Dive,
	/**
	 * 착지점이 지금 높이보다 위라 곧장 꽂을 수 없다. 착지점 바로 위까지 건너간 뒤 Dive로 넘어간다.
	 *
	 * 위로 향하는 직선은 바닥을 위에서 만나지 못한다: 그대로 지나쳐 허공으로 날아가 버리므로
	 * 착지 판정이 오지 않는다. 그래서 높은 곳은 넘어가서 떨어뜨린다.
	 */
	Approach,
	/**
	 * 착지 직후의 경직. LandingRecoverTime 동안 움직일 수 없고, 그동안 착지 동작이 돈다.
	 *
	 * 곧바로 None으로 가지 않는 이유는 "못 움직인다"는 판단이 이미 단계에 걸려 있기 때문이다
	 * (IsInputLocked). 시간을 따로 재면 서버와 클라이언트의 시계가 갈라져 고무줄이 난다.
	 */
	Recover,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeroLandingPhaseChanged, EHeroLandingPhase /*NewPhase*/);

/** 히어로 랜딩의 튜닝값. 프로필에서 오고, 어빌리티가 시작할 때 양쪽 무브먼트에 같은 값을 넣는다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FHeroLandingParams
{
	GENERATED_BODY()

	/** 이륙점에서 얼마나 올라가는지(cm). 점프대(1200 cm/s)의 정점 735 cm가 기본. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RiseHeight = 735.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float RiseTime = 0.6f;

	/** 정점에서 멈춰 조준하는 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "s"))
	float HoverTime = 2.0f;

	/** 이륙점에서 착지점까지의 최대 수평 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxAimDistance = 1500.0f;

	/** 내리꽂히는 속도(cm/s). 높은 곳으로 건너갈 때도 같은 속도로 움직인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "100", ForceUnits = "cm/s"))
	float DiveSpeed = 3000.0f;

	/**
	 * 착지점이 지금 높이보다 위일 때, 그 위 이만큼까지 건너간 다음 수직으로 떨어진다(cm).
	 * 0이면 착지점 바로 위에서 떨어지므로 난간이나 턱에 걸리기 쉽다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DiveApexClearance = 150.0f;

	/**
	 * 착지 뒤 움직이지 못하는 시간(초). 착지 동작이 이 시간 동안 돈다.
	 *
	 * 애님 블루프린트의 착지 상태도 이 단계(Recover)를 보고 들어오고 나가므로, 이 값을 클립
	 * 길이에 맞추면 동작이 잘리거나 마지막 포즈로 멈춰 있는 일이 없다. 0이면 경직 없이 곧바로 움직인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "s"))
	float LandingRecoverTime = 0.5f;

	/** 조준 트레이스 길이(cm). 이보다 먼 곳을 보면 이륙 높이의 수평면과 만나는 점을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "100", ForceUnits = "cm"))
	float AimTraceDistance = 4000.0f;
};

/**
 * 대시(지속형 이동 가속)를 클라이언트 예측으로 처리하는 무브먼트 컴포넌트.
 *
 * MaxWalkSpeed를 직접 바꾸면 원격 클라이언트에서 고무줄 현상이 난다. 클라이언트는
 * 바뀐 속도로 시뮬레이션하는데 서버는 자기 값으로 다시 시뮬레이션하므로 위치가
 * 벌어지고, 서버가 ClientAdjustPosition으로 되돌리기 때문이다. MaxWalkSpeed는
 * 복제 프로퍼티도 아니라서 서버는 클라이언트가 무엇을 했는지조차 모른다.
 *
 * 그래서 속도가 아니라 "대시하고 싶다"는 의도만 압축 플래그에 실어 보낸다. 서버와
 * 보정 후 리플레이 경로가 같은 플래그로 같은 계산을 하므로 결과가 어긋나지 않는다.
 *
 * 히어로 랜딩도 같은 이유로 여기 있다. 어빌리티가 SetMovementMode나 Velocity를 직접 쓰면
 * 서버는 클라이언트의 모드를 받아들이지 않고, 보정 뒤 리플레이가 그 값을 덮어쓴다. 대신
 * 의도(FLAG_Custom_2)와 단계 상태를 저장 무브에 실어 양쪽이 같은 단계 기계를 돌린다.
 *
 * 몸통 Yaw도 여기서 돌린다. 둘러볼 때는 그대로 두고, 이동 입력이 있거나 쏘는 동안에만
 * 컨트롤 Yaw를 향해 RotationRate로 돈다(bUseControllerDesiredRotation 경로를 게이트로 감쌈).
 * 게이트에 압축 플래그는 없다: 가속과 컨트롤 회전은 무브에 이미 실려 오고, 발사 상태는
 * 복제된 충전 플래그와 발사 알림으로 서버도 알기 때문에 양쪽이 같은 답을 낸다.
 */
UCLASS()
class MINTCHOCO_API UUnitMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** MOVE_Custom의 서브 모드. 상승과 정지 단계가 이 모드에서 돈다. */
	static constexpr uint8 CustomMode_HeroLanding = 1;

	UUnitMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;

	/**
	 * 내리꽂는 동안에는 0. 낙하 모드로 직선을 그어야 조준한 지점에 정확히 떨어진다.
	 * 중력이 걸리면 궤적이 아래로 휘어 표시된 착지점보다 앞에서 땅에 닿는다.
	 */
	virtual float GetGravityZ() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	/** ShouldFaceControlRotation이 참일 때만 엔진의 컨트롤 회전 추종을 돌린다. 거짓이면 몸통을 건드리지 않는다. */
	virtual void PhysicsRotation(float DeltaTime) override;

	/**
	 * 이번 무브에서 몸통이 컨트롤 Yaw를 향해 돌아야 하는지: 이동 가속이 있거나 유닛이 조준 중이고,
	 * 입력이 잠기지 않았을 때(스턴·히어로 랜딩 중에는 돌지 않는다).
	 */
	bool ShouldFaceControlRotation() const;

	/**
	 * 로컬 입력이 부른다. 서버에는 다음 무브의 압축 플래그에 실려 전달되므로
	 * 별도의 RPC가 필요 없다.
	 */
	void SetWantsToDash(bool bNewWantsToDash);

	/** 실제로 상태가 바뀔 때만 발생한다. 연출과 복제용 플래그가 여기에 물린다. */
	FOnDashStateChanged OnDashStateChanged;

	/**
	 * 아이템(스피드 스타)의 속도 부스트 의도. 대시와 같은 경로로 압축 플래그에 실린다.
	 *
	 * 소유 클라이언트에서는 슬롯 컴포넌트가 효과 태그를 보고 세우고, 서버는
	 * UpdateFromCompressedFlags에서 되살린다. GAS의 속성 예측으로 속도를 바꾸면 서버가
	 * 효과를 적용하기 전(RTT/2)의 무브를 옛 속도로 시뮬레이션해 보정이 나므로, 속도는
	 * GE가 아니라 이 플래그가 정한다.
	 */
	void SetWantsSpeedBoost(bool bNewWantsSpeedBoost);

	bool WantsSpeedBoost() const { return bWantsSpeedBoost != 0; }

	FOnSpeedBoostStateChanged OnSpeedBoostStateChanged;

	//~ 히어로 랜딩

	/**
	 * 히어로 랜딩 의도(FLAG_Custom_2). 어빌리티가 소유 클라이언트와 서버에서 세운다. 플래그가
	 * 0에서 1로 바뀌는 무브에서 상승이 시작되고, 착지하면 무브먼트가 스스로 내린다.
	 */
	void SetWantsHeroLanding(bool bNewWantsHeroLanding);
	bool WantsHeroLanding() const { return bWantsHeroLanding != 0; }

	/**
	 * "지금 내리꽂자"는 의도(FLAG_Custom_3). 상승·정지 중에 소유 클라이언트가 좌클릭하면 선다.
	 *
	 * 정지 단계는 원래 HoverTime이 다 지나야 끝나는데, 이 플래그가 서 있으면 그 자리에서
	 * 내리꽂는다. HoverTime은 그대로 두어 아무것도 누르지 않아도 언젠가는 떨어지게 한다.
	 *
	 * 랜딩 의도와 같은 이유로 압축 플래그다: 단계를 굴리는 것은 무브먼트이고, 서버는 무브에
	 * 실려 온 플래그로 같은 계산을 해야 궤적이 어긋나지 않는다. 내리꽂기가 시작되면 내려간다.
	 */
	void SetWantsHeroDive(bool bNewWantsHeroDive) { bWantsHeroDive = bNewWantsHeroDive ? 1 : 0; }
	bool WantsHeroDive() const { return bWantsHeroDive != 0; }

	/**
	 * 정지 단계를 얼마나 버텼는지(0~1). 내리꽂기가 시작되는 순간 굳는다.
	 *
	 * 시간으로만 정해지므로 서버가 스스로 안다 — 클라이언트가 보내 주는 값이 아니라서
	 * 조작할 여지가 없고, 복제할 것도 없다. 착지 효과의 배율이 여기서 나온다.
	 */
	UFUNCTION(BlueprintPure, Category = "HeroLanding")
	float GetHeroCharge() const;

	/** 시작 전에 양쪽이 같은 값을 넣어야 같은 궤적이 나온다. 단계 중에 바꿔도 다음 발동부터다. */
	void SetHeroLandingParams(const FHeroLandingParams& Params) { HeroParams = Params; }
	const FHeroLandingParams& GetHeroLandingParams() const { return HeroParams; }

	UFUNCTION(BlueprintPure, Category = "HeroLanding")
	EHeroLandingPhase GetHeroLandingPhase() const { return HeroPhase; }

	/**
	 * 단계가 실제로 바뀔 때만. 이 값은 압축 플래그로 굴러가 소유자와 서버에만 있으므로,
	 * AUnit이 여기서 받아 원격 클라이언트에 복제한다(대시와 같은 이유). 보정 후 리플레이는
	 * 이미 지나간 시간을 다시 계산하는 것이라 알리지 않는다.
	 */
	FOnHeroLandingPhaseChanged OnHeroLandingPhaseChanged;

	/** 내리꽂기 단계에서 고정된 착지점. 그 전에는 ComputeAimTarget()이 지금 조준하는 곳이다. */
	FVector GetHeroDiveTarget() const { return HeroDiveTarget; }

	/**
	 * 지금 컨트롤 회전이 가리키는 착지점. 내려설 수 있는 곳을 보고 있을 때만 참이다.
	 *
	 * 눈높이에서 시선으로 트레이스해 **걸을 수 있는 바닥**을 맞혔을 때만 인정한다. 벽, 급경사,
	 * 사거리 밖, 아무것도 없는 허공은 전부 거짓이고, 그러면 착지점 표시도 뜨지 않고 좌클릭도
	 * 듣지 않는다. 높이는 자르지 않으므로 지금 서 있는 곳보다 높은 바닥도 고를 수 있다.
	 *
	 * 서버는 무브에 실려 온 컨트롤 회전으로 같은 계산을 한다.
	 */
	bool ComputeAimTarget(FVector& OutTarget) const;

	/**
	 * 착지 알림(ACharacter::Landed)에서 유닛이 부른다. 내리꽂기 중이었으면 단계를 끝내고
	 * 플래그를 내리며 true. 아니면(보통 착지) false.
	 */
	bool FinishHeroLandingDive();

	/** 어빌리티가 상한 시간이나 취소로 끝날 때. 진행 중인 단계를 버리고 낙하로 돌아간다. */
	void AbortHeroLanding();

	/**
	 * 시뮬레이션 프록시 전용. 복제된 단계를 그대로 받는다(AUnit이 넣어 준다).
	 *
	 * 프록시는 단계 기계를 돌리지 않으므로 단계를 알 길이 없는데, 내리꽂기 중 중력을 끄는
	 * 판단이 거기 걸려 있다. 넣어 주지 않으면 프록시만 중력을 더 받아 서버보다 빨리 가라앉는다.
	 * 단계를 직접 굴리는 쪽(소유자, 서버)에서는 아무 일도 하지 않는다.
	 */
	void SetSimulatedHeroLandingPhase(EHeroLandingPhase NewPhase);

	/**
	 * 부스트 중의 이동 속도(cm/s). 배율이 아니라 고정값이다: "감속 지대 무시"가 규칙이라
	 * 기본 속도에 무엇이 곱해지든 이 값으로 달린다. 대시도 여기에 곱해지지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SpeedBoostSpeed = 1500.0f;

	/**
	 * 기본 이동 속도에 곱해지는 배율.
	 *
	 * 절대값이 아니라 배율인 이유는, 나중에 아이템 이동속도 버프가 붙었을 때
	 * 자연스럽게 함께 곱해지기 때문이다. 절대값이면 대시 중에만 버프가 사라진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "1.0"))
	float DashSpeedMultiplier = 1.7f;

private:
	friend class FSavedMove_Unit;

	/**
	 * 대시 의도. 복제 프로퍼티가 아니다. 소유 클라이언트가 세우고 압축 플래그로
	 * 서버에 전달되며, 서버는 UpdateFromCompressedFlags에서 되살린다.
	 */
	uint8 bWantsToDash : 1;

	/** 속도 부스트 의도. 대시와 같은 규칙으로 다뤄진다. */
	uint8 bWantsSpeedBoost : 1;

	/** 히어로 랜딩 의도. 같은 규칙. */
	uint8 bWantsHeroLanding : 1;

	/**
	 * 플래그가 0인 무브를 본 뒤에만 다음 1이 상승을 시작한다. 착지 직후 아직 1인 무브가
	 * 몇 개 더 오는데(클라이언트가 서버보다 늦게 착지한 경우), 그것으로 다시 뜨면 안 된다.
	 */
	uint8 bHeroLandingArmed : 1;

	/** 정지 단계를 지금 끝내고 내리꽂겠다는 의도. 내리꽂기가 시작되면 스스로 내려간다. */
	uint8 bWantsHeroDive : 1;

	/** 내리꽂기가 시작될 때 굳은 충전량(0~1). 그 전에는 GetHeroCharge()가 시간으로 센다. */
	float HeroCharge = 0.0f;

	EHeroLandingPhase HeroPhase = EHeroLandingPhase::None;
	float HeroPhaseTime = 0.0f;
	FVector HeroTakeoff = FVector::ZeroVector;
	FVector HeroDiveTarget = FVector::ZeroVector;

	/** Approach 단계가 향하는 점. 착지점 바로 위 DiveApexClearance만큼 높은 곳이다. */
	FVector HeroApproachPoint = FVector::ZeroVector;

	FHeroLandingParams HeroParams;

	/** 서버가 클라이언트의 부스트 플래그를 인정해도 되는지 유닛에게 묻는다. 유닛이 아니면 항상 참. */
	bool IsSpeedBoostAllowed() const;

	/** 서버가 클라이언트의 히어로 랜딩 플래그를 인정해도 되는지. 효과 태그가 있거나 그 아이템을 들고 있을 때. */
	bool IsHeroLandingAllowed() const;

	/** 입력으로 움직일 수 없는 상태인지(스턴, 히어로 랜딩 단계). 유닛이 답하고, 유닛이 아니면 단계만 본다. */
	bool IsInputLocked() const;

	/** 유닛이 쏘거나 충전 중이라 조준 방향을 봐야 하는지. 유닛이 아니면 거짓. */
	bool IsAimHeld() const;

	/**
	 * 이 컴포넌트가 시뮬레이션 프록시(남의 화면에 보이는 남의 캐릭터)의 것인지.
	 *
	 * 그쪽에는 의도 플래그도 단계도 없다. 둘 다 압축 플래그를 타고 소유자와 서버에만 닿기
	 * 때문이다. 위치는 복제가 끌고 가므로 단계 기계를 돌릴 이유도 없다.
	 */
	bool IsSimulatedProxy() const;

	/** 값이 실제로 바뀔 때만 알린다. 단계는 이 함수로만 바꾼다(리플레이 복원은 예외). */
	void SetHeroPhase(EHeroLandingPhase NewPhase);

	void StartHeroLanding();
	void PhysHeroLanding(float DeltaTime, int32 Iterations);

	/** 착지점이 위면 Approach로, 아래면 곧장 Dive로. 정지 단계가 끝날 때 한 번 부른다. */
	void StartHeroDive(const FVector& Target, float DeltaTime, int32 Iterations);

	/** 착지점 위까지 직선으로 건너간다. 도착하거나 막히면 수직 낙하로 넘어간다. */
	void PhysHeroApproach(float DeltaTime, int32 Iterations);

	/** 수직 낙하로 전환한다. 낙하 모드라 착지가 ProcessLanded → ACharacter::Landed로 온다. */
	void StartHeroPlunge(float DeltaTime, int32 Iterations);

	/** 캡슐 반높이(cm). 캡슐이 없으면 0. */
	float GetHeroCapsuleHalfHeight() const;
};

/**
 * 무브 하나에 대시 의도를 함께 저장한다.
 *
 * 서버 보정이 일어나면 클라이언트는 저장해둔 무브들을 처음부터 다시 재생하는데,
 * 그때 대시 상태도 같이 복원되어야 결과가 원래 시뮬레이션과 일치한다. 히어로 랜딩의
 * 단계 상태도 같은 이유로 통째로 실린다.
 */
class FSavedMove_Unit : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

private:
	uint8 bSavedWantsToDash : 1;
	uint8 bSavedWantsSpeedBoost : 1;
	uint8 bSavedWantsHeroLanding : 1;
	uint8 bSavedHeroLandingArmed : 1;
	uint8 bSavedWantsHeroDive : 1;
	float SavedHeroCharge = 0.0f;
	EHeroLandingPhase SavedHeroPhase = EHeroLandingPhase::None;
	float SavedHeroPhaseTime = 0.0f;
	FVector SavedHeroTakeoff = FVector::ZeroVector;
	FVector SavedHeroDiveTarget = FVector::ZeroVector;
	FVector SavedHeroApproachPoint = FVector::ZeroVector;
};

class FNetworkPredictionData_Client_Unit : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Unit(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
