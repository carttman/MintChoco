// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnitMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDashStateChanged, bool /*bDashing*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpeedBoostStateChanged, bool /*bBoosting*/);

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
 */
UCLASS()
class MINTCHOCO_API UUnitMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UUnitMovementComponent();

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

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

	/** 서버가 클라이언트의 부스트 플래그를 인정해도 되는지 유닛에게 묻는다. 유닛이 아니면 항상 참. */
	bool IsSpeedBoostAllowed() const;
};

/**
 * 무브 하나에 대시 의도를 함께 저장한다.
 *
 * 서버 보정이 일어나면 클라이언트는 저장해둔 무브들을 처음부터 다시 재생하는데,
 * 그때 대시 상태도 같이 복원되어야 결과가 원래 시뮬레이션과 일치한다.
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
};

class FNetworkPredictionData_Client_Unit : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Unit(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
