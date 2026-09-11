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
};

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

	/** 내리꽂히는 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeroLanding", meta = (ClampMin = "100", ForceUnits = "cm/s"))
	float DiveSpeed = 3000.0f;

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

	/** 시작 전에 양쪽이 같은 값을 넣어야 같은 궤적이 나온다. 단계 중에 바꿔도 다음 발동부터다. */
	void SetHeroLandingParams(const FHeroLandingParams& Params) { HeroParams = Params; }
	const FHeroLandingParams& GetHeroLandingParams() const { return HeroParams; }

	UFUNCTION(BlueprintPure, Category = "HeroLanding")
	EHeroLandingPhase GetHeroLandingPhase() const { return HeroPhase; }

	/** 내리꽂기 단계에서 고정된 착지점. 그 전에는 ComputeAimTarget()이 지금 조준하는 곳이다. */
	FVector GetHeroDiveTarget() const { return HeroDiveTarget; }

	/**
	 * 지금 컨트롤 회전이 가리키는 착지점. 눈높이에서 시선으로 트레이스하고, 안 맞으면 이륙
	 * 높이의 수평면과 만나는 점을 쓰며, 이륙점 기준 수평 거리를 MaxAimDistance로 자른다.
	 * 서버는 무브에 실려 온 컨트롤 회전으로 같은 계산을 한다.
	 */
	FVector ComputeAimTarget() const;

	/**
	 * 착지 알림(ACharacter::Landed)에서 유닛이 부른다. 내리꽂기 중이었으면 단계를 끝내고
	 * 플래그를 내리며 true. 아니면(보통 착지) false.
	 */
	bool FinishHeroLandingDive();

	/** 어빌리티가 상한 시간이나 취소로 끝날 때. 진행 중인 단계를 버리고 낙하로 돌아간다. */
	void AbortHeroLanding();

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

	EHeroLandingPhase HeroPhase = EHeroLandingPhase::None;
	float HeroPhaseTime = 0.0f;
	FVector HeroTakeoff = FVector::ZeroVector;
	FVector HeroDiveTarget = FVector::ZeroVector;
	FHeroLandingParams HeroParams;

	/** 서버가 클라이언트의 부스트 플래그를 인정해도 되는지 유닛에게 묻는다. 유닛이 아니면 항상 참. */
	bool IsSpeedBoostAllowed() const;

	/** 서버가 클라이언트의 히어로 랜딩 플래그를 인정해도 되는지. 효과 태그가 있거나 그 아이템을 들고 있을 때. */
	bool IsHeroLandingAllowed() const;

	/** 입력으로 움직일 수 없는 상태인지(스턴, 히어로 랜딩 단계). 유닛이 답하고, 유닛이 아니면 단계만 본다. */
	bool IsInputLocked() const;

	void StartHeroLanding();
	void PhysHeroLanding(float DeltaTime, int32 Iterations);
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
	EHeroLandingPhase SavedHeroPhase = EHeroLandingPhase::None;
	float SavedHeroPhaseTime = 0.0f;
	FVector SavedHeroTakeoff = FVector::ZeroVector;
	FVector SavedHeroDiveTarget = FVector::ZeroVector;
};

class FNetworkPredictionData_Client_Unit : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Unit(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
