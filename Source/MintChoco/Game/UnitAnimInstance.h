#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "Game/UnitMovementComponent.h"

#include "UnitAnimInstance.generated.h"

class AUnit;

/** 애니메이션 변수 계산의 순수 부분. 월드 없이 테스트한다. */
struct MINTCHOCO_API FUnitAnimMath
{
	/** 월드 속도를 캐릭터 기준 앞(X)·오른쪽(Y) 성분으로. Z는 버린다. */
	static FVector2D LocalPlanarSpeed(const FVector& Velocity, const FRotator& ActorRotation);

	/**
	 * 이동 방향 각(도, -180~180). 0이 앞, 90이 오른쪽, ±180이 뒤. 수평 속도가 없으면 0.
	 * 1D 블렌드스페이스에 방향 축을 줄 때 쓴다.
	 */
	static float MoveDirectionDegrees(const FVector& Velocity, const FRotator& ActorRotation);
};

/**
 * 유닛 애님 블루프린트의 부모. 블렌드스페이스와 스테이트 머신이 읽을 변수를 매 프레임 채운다.
 *
 * 값은 전부 폰의 컴포넌트에서 읽는다. 속도는 무브먼트의 Velocity다: 위치 차분으로 재면 리슨
 * 호스트에서 원격 폰이 ServerMove가 올 때만 움직여 슬로모션으로 보인다(CLAUDE.md의 함정).
 * 상태(대시·스턴·히어로 랜딩)는 이미 복제되는 값이라 모든 머신에서 같은 포즈가 나온다.
 *
 * AnimGraph는 에디터에서 그린다. 이 클래스는 변수만 준다.
 */
UCLASS()
class MINTCHOCO_API UUnitAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	//~ 이동

	/** 캐릭터 기준 앞쪽 속도(cm/s). 뒤로 걸으면 음수. 2D 블렌드스페이스의 세로축. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	float MoveForward = 0.0f;

	/** 캐릭터 기준 오른쪽 속도(cm/s). 왼쪽이면 음수. 2D 블렌드스페이스의 가로축. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	float MoveRight = 0.0f;

	/** 수평 속력(cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	float GroundSpeed = 0.0f;

	/** 이동 방향 각(도, -180~180). 0 앞, 90 오른쪽, ±180 뒤. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	float MoveDirection = 0.0f;

	/** 수평 속력이 MovingSpeedThreshold를 넘는지. Idle↔Move 전이. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	bool bIsMoving = false;

	/** 이동 입력이 들어오고 있는지(가속 중). 스타트·스톱 구분. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Move")
	bool bHasMoveInput = false;

	//~ 공중

	UPROPERTY(BlueprintReadOnly, Category = "Unit|Air")
	bool bIsInAir = false;

	/** 수직 속도(cm/s). 올라가면 양수. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Air")
	float VerticalSpeed = 0.0f;

	//~ 상태

	/** 대시 중. 소유자는 예측값, 나머지는 복제값. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	bool bIsDashing = false;

	/** 스턴 중(State.Status.Stunned). */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	bool bIsStunned = false;

	/** 히어로 랜딩 단계. None이면 평소. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	EHeroLandingPhase HeroLandingPhase = EHeroLandingPhase::None;

	//~ 조준·사격

	/**
	 * 카메라 상하 각(도, -90~90). APawn::GetBaseAimRotation: 로컬은 컨트롤 회전, 원격은 복제된 시점 피치.
	 * 원격 폰은 복제 주기로 계단식으로 오므로 AimPitchInterpSpeed로 보간한 값이다. 로컬 폰은 그대로. 에임 오프셋용.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Aim")
	float AimPitch = 0.0f;

	/** 주무기 방아쇠가 당겨져 있는지. 소유 머신에서만 참이 된다(방아쇠는 복제되지 않는다). */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Aim")
	bool bIsFiring = false;

	//~ 튜닝

	/** 이 속력(cm/s)을 넘어야 "이동 중"이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MovingSpeedThreshold = 10.0f;

	/** 이 값(cm/s²) 이상의 가속이 있어야 "입력 중"이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0"))
	float InputAccelerationThreshold = 10.0f;

	/** 원격 폰의 AimPitch 보간 속도(FInterpTo). 0이면 보간 없이 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0"))
	float AimPitchInterpSpeed = 18.0f;

private:
	/** 소유 폰. 유닛이 아니면 이동·공중 값만 채우고 상태는 기본값으로 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<AUnit> Unit;

	/** 첫 업데이트에서는 보간 없이 맞춘다(0에서 미끄러져 올라오지 않게). */
	bool bAimPitchInitialized = false;
};
