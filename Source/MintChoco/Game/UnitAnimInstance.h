#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "Game/UnitMovementComponent.h"

#include "UnitAnimInstance.generated.h"

class AUnit;
class UAnimSequenceBase;

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

	/**
	 * 마지막 발사 후 HoldSeconds가 아직 지나지 않았는지. LastFiredTime이 음수면 한 번도 쏘지 않은 것이다.
	 * HoldSeconds가 0 이하면 항상 거짓.
	 */
	static bool IsFireHoldActive(double Now, double LastFiredTime, float HoldSeconds);

	/**
	 * 요가 한 프레임 동안 돈 속도(도/초). 오른쪽으로 돌면(요 증가) 양수다. 경계(±180, 0/360)를 넘거나
	 * 값이 360을 넘게 쌓여 있어도 짧은 쪽으로 잰다. DeltaSeconds가 0이면 0.
	 */
	static float YawRateDegrees(double PreviousYaw, double CurrentYaw, float DeltaSeconds);

	/**
	 * 보드 기울기(도). 도는 자전거나 보드가 기우는 각 atan(v·ω/g)을 따른다: 빨리 달리며 급하게 돌수록
	 * 크게, 느리거나 완만하게 돌면 적게 기운다. 멈춰서 돌면 0이다. 오른쪽으로 돌면(요 증가) 오른쪽(+),
	 * 곧 회전 안쪽으로 기운다. LeanGravity(cm/s²)가 클수록 덜 기울고, 결과는 ±|MaxDegrees|로 자른다.
	 * MaxDegrees가 음수면 방향을 뒤집는다. LeanGravity가 0 이하면 0.
	 */
	static float BoardLeanFromTurn(float GroundSpeed, float YawRateDegreesPerSecond, float LeanGravity, float MaxDegrees);

	/**
	 * 히어로 랜딩 다이브 중 메시를 캡슐과 따로 돌릴 각(캡슐 축 기준, 도). 내리꽂는 동안 캡슐은 전혀
	 * 돌지 않으므로(UUnitMovementComponent::ShouldFaceControlRotation이 입력 잠금에서 먼저 막힌다)
	 * 뜬 자리에서 보던 쪽을 그대로 본 채 옆으로 미끄러진다. 그 차이를 메시가 메운다.
	 *
	 * 요는 다이브 방향과 몸이 보는 쪽(BodyYaw)의 차이이고, 피치는 다이브 벡터가 눕는 각을
	 * ±|MaxPitchDegrees|로 자른 값이다(내려가므로 음수, 곧 앞으로 숙인다).
	 *
	 * 수평 성분이 없는 수직 낙하(착지점 위로 건너간 뒤의 StartHeroPlunge)에는 향할 방향이 없다.
	 * 그때는 CurrentTiltYaw를 그대로 돌려준다 — 새로 구하면 몸이 세계 기준 0도로 홱 돈다.
	 * 속도가 0이면 영 회전.
	 */
	static FRotator HeroDiveTilt(const FVector& Velocity, double BodyYaw, float CurrentTiltYaw, float MaxPitchDegrees);
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
	virtual void NativeUninitializeAnimation() override;

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

	/**
	 * 로코모션 상태 기계가 대시 상태(이름이 DashStatePrefix로 시작하는 Dash_Start/Loop/End)에
	 * 있는지. bIsDashing이 "키를 누르고 있다"라면 이것은 "보드 동작이 실제로 돌고 있다"이다.
	 * 전이가 시작되는 프레임부터 참이라 보드(AUnit::SetBoardShown)가 동작과 함께 나타나고
	 * 끝 동작이 끝나야 사라진다. 한 프레임 전 상태를 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	bool bDashAnimationActive = false;

	/**
	 * 보드 기울기(도). 양수면 오른쪽으로 기운다. 보드 동작 중에만 보이는 몸이 도는 속도와 이동 속도만큼
	 * 회전 안쪽으로 기울고(FUnitAnimMath::BoardLeanFromTurn), 돌지 않거나 보드에서 내리면 0으로
	 * 돌아온다. 이 값은 다이브 기울기와 합쳐져 유닛의 메시 회전이 된다(AUnit::SetMeshOffset).
	 * 애님 그래프가 따로 쓸 일은 없지만 디버깅용으로 읽을 수 있게 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Dash")
	float BoardLean = 0.0f;

	/**
	 * 히어로 랜딩 다이브 기울기(도). 내리꽂는 동안 메시가 다이브 방향을 향하도록 돌아간 각이고,
	 * 다이브가 아니면 0으로 돌아온다(FUnitAnimMath::HeroDiveTilt). 보드 기울기와 마찬가지로
	 * 복제하지 않는다: 단계와 속도가 이미 복제되므로 머신마다 같은 값이 나온다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|HeroLanding")
	FRotator HeroDiveTilt = FRotator::ZeroRotator;

	/**
	 * 히어로 랜딩 단계. None이면 평소.
	 *
	 * 준비(Rise·Hover), 건너가기(Approach), 내리꽂기(Dive), 착지 경직(Recover) 자세를 여기서
	 * 고른다. 착지 동작은 Recover 동안 돌면 되고, 그 길이는 LandingRecoverTime이 정한다 —
	 * 그동안은 움직일 수도 없으므로 동작과 조작이 어긋나지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	EHeroLandingPhase HeroLandingPhase = EHeroLandingPhase::None;

	/**
	 * 히어로 랜딩 중인지(단계가 None이 아닌지). 상체 레이어를 끄는 조건으로 쓴다.
	 *
	 * 히어로 랜딩 자세는 로코모션 스테이트 머신 안에 있어 상체 레이어보다 위에 있다. 그래서
	 * 발사 직후 FireHoldTime 동안은 조준 상체가 랜딩 자세를 덮어쓴다. 랜딩 중에는 방아쇠가
	 * 막혀 있으므로(State.Item.HeroLanding) 상체 조준을 켤 이유가 없다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|State")
	bool bIsHeroLanding = false;

	//~ 아이템

	/**
	 * 효과가 도는(또는 조준 중인) 아이템의 유지 자세. 없으면 nullptr.
	 *
	 * 시퀀스 플레이어의 Sequence 핀에 바인딩하고 Loop를 켜서 쓴다. 아이템이 몇 개로 늘어도
	 * 애님 그래프는 그대로이고, 새 아이템은 프로필의 PoseAnimation을 채우는 것으로 끝난다.
	 * 상태 태그(와 조준 플래그)에서 오므로 모든 머신에서 같은 자세가 나온다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Item")
	TObjectPtr<UAnimSequenceBase> ItemPose;

	/** 유지할 아이템 자세가 있는지(덮는 범위와 무관). */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Item")
	bool bHasItemPose = false;

	/**
	 * 전신을 덮는 유지 자세가 있는지. 전신 블렌드의 조건.
	 *
	 * 범위별로 따로 내보내는 이유는 애님 그래프에서 AND·NOT을 엮지 않게 하기 위해서다. 둘은
	 * 동시에 참이 되지 않으므로 두 가지를 차례로 물려도 한 번에 하나만 켜진다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Item")
	bool bHasFullBodyItemPose = false;

	/** 상체만 덮는 유지 자세가 있는지. 상체 블렌드의 조건. */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Item")
	bool bHasUpperBodyItemPose = false;

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

	/**
	 * 주무기나 보조 무기가 마지막으로 한 발 쏜 뒤 FireHoldTime초 동안 참. 쏠 때마다 다시 늘어난다.
	 *
	 * 무기의 OnFired를 받으므로 모든 머신에서 같다: 소유자는 예측 발사 순간, 서버는 실제 발사,
	 * 다른 클라이언트는 샷 멀티캐스트가 도착한 순간. 짧게 클릭한 단발도 이 시간만큼 조준 자세가
	 * 유지된다. 상체 에임 오프셋 전환(Blend Poses by bool)에 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Aim")
	bool bRecentlyFired = false;

	/**
	 * 무기 하나라도 지금 조준 자세를 요구하는 중인지. 방아쇠를 당긴 순간부터 놓을 때까지,
	 * 그리고 차지샷을 충전하는 내내 참이다(UPaintWeaponComponent::IsAiming).
	 *
	 * bRecentlyFired 가 쏜 **뒤**의 여운을 맡는다면 이쪽은 쏘기 **전**과 충전 **중**을 맡는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Aim")
	bool bIsAiming = false;

	/**
	 * **상체 조준 자세를 켜고 끄는 값. 애님 그래프는 이것 하나만 보면 된다.**
	 *
	 * bIsAiming(쏘기 전 · 충전 중)과 bRecentlyFired(쏜 뒤 FireHoldTime)를 합친 것이다.
	 * 그래서 자세는 방아쇠를 당기는 순간 올라가 충전 내내 유지되고, 쏜 뒤에도 잠시 남았다가
	 * 내려온다 — 총이 사라지는 시점(GunVisibleHoldTime)과도 맞는다. 대시 중에는 항상 거짓이다:
	 * 보드 위에서는 쏘지 못하므로 여운이 보드 자세를 덮을 이유가 없다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Unit|Aim")
	bool bWeaponPoseHeld = false;

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

	/** 마지막 발사 후 bRecentlyFired를 유지하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0", ForceUnits = "s"))
	float FireHoldTime = 0.5f;

	/** 대시 상태를 찾을 상태 기계의 이름. 애님 그래프의 상태 기계 노드 이름과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning")
	FName LocomotionMachineName = TEXT("Locomotion");

	/** 이 접두사로 시작하는 상태가 대시 동작이다(Dash_Start, Dash_Loop, Dash_End). */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning")
	FString DashStatePrefix = TEXT("Dash");

	/**
	 * 보드 동작 중 최대 기울기(도). 캐릭터의 앞 축을 중심으로 메시를 굴린다. 기우는 방향이 반대로
	 * 보이면 부호를 바꾼다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "-45", ClampMax = "45", ForceUnits = "deg"))
	float BoardLeanMaxDegrees = 25.0f;

	/**
	 * 보드 기울기 식 atan(속도 × 각속도 / 이 값)에서 중력 자리(cm/s²). 클수록 덜 기운다. 기본값이면 대시
	 * 속도(1700 cm/s)로 90도/초 돌 때 약 15도, 30도/초면 약 5도 기운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "1"))
	float BoardLeanGravity = 10000.0f;

	/** 보드 기울기가 목표로 따라가는 속도(FInterpTo). 클수록 빨리 기운다. 0이면 보간 없이 바로 기운다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0"))
	float BoardLeanInterpSpeed = 8.0f;

	/**
	 * 히어로 랜딩 다이브에서 앞으로 숙이는 최대 각(도). 다이브 방향에 그대로 맞추면 수직 낙하에서
	 * 90도로 완전히 엎어지므로 여기서 자른다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float HeroDiveTiltMaxPitch = 60.0f;

	/** 다이브 기울기가 목표로 따라가는 속도(FInterpTo). 착지해서 다이브가 끝나면 같은 속도로 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Unit|Tuning", meta = (ClampMin = "0"))
	float HeroDiveTiltInterpSpeed = 15.0f;

private:
	/** 소유 폰. 유닛이 아니면 이동·공중 값만 채우고 상태는 기본값으로 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<AUnit> Unit;

	/** 첫 업데이트에서는 보간 없이 맞춘다(0에서 미끄러져 올라오지 않게). */
	bool bAimPitchInitialized = false;

	/** 지난 프레임에 보인 몸의 요(도)와 그 값이 유효한지. 보드 기울기의 회전 속도를 잰다. */
	double LastBodyYaw = 0.0;
	bool bBodyYawInitialized = false;

	/**
	 * 지난 프레임의 히어로 랜딩 단계. 내리꽂기가 끝나는 프레임을 잡으려는 것뿐이다: 그때 캡슐이
	 * 꽂은 쪽으로 돌아서므로 메시는 들고 있던 요를 놓아야 한다.
	 */
	EHeroLandingPhase LastHeroLandingPhase = EHeroLandingPhase::None;

	/** 보이는 몸의 요(도). 메시 월드 회전에서 기준 회전을 걷어 낸 값이다. 모든 머신에서 같은 식으로 잰다. */
	double GetBodyYaw(const AUnit& InUnit) const;

	/** 두 무기의 OnFired에서. 이 머신의 월드 시각을 기록한다. */
	UFUNCTION()
	void HandleWeaponFired(int32 Seed);

	/** 발사 알림을 받을 유닛을 바꾼다. 옛 유닛의 무기에서는 풀고 새 유닛의 무기에 건다. nullptr이면 풀기만 한다. */
	void BindWeapons(AUnit* NewUnit);

	/** 발사 알림을 걸어 둔 유닛. 폰이 바뀌거나 사라지면 NativeUpdateAnimation이 다시 건다. */
	TWeakObjectPtr<AUnit> BoundUnit;

	/** 이 머신에서 마지막 발사를 본 월드 시각. 음수면 아직 없다. */
	double LastFiredTime = -1.0;

	/** LocomotionMachineName의 상태 기계 인덱스. 초기화 때 한 번 찾는다. 없으면 INDEX_NONE. */
	int32 LocomotionMachineIndex = INDEX_NONE;

	/** 현재 로코모션 상태가 대시 상태인지. */
	bool IsInDashState() const;
};
