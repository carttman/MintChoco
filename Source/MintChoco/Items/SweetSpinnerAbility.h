#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SweetSpinnerAbility.generated.h"

class AUnit;
class UAnimSequenceBase;
class USweetSpinnerProfile;
enum class EItemPoseBlend : uint8;

/**
 * 스위트 스피너. 캐릭터는 돌지 않고, 서버가 일정 간격으로 쏘는 산탄의 방향만 시작 요에서
 * 출발해 Turns바퀴를 고르게 나눠 돈다(SweetSpinner::VolleyYawDegrees). 캐릭터가 도는 모습은
 * 애니메이션이 맡으므로 액터 회전과 컨트롤 요 추종은 건드리지 않는다.
 *
 * 산탄은 서버만 쏜다. 무기의 ServerFire와 달리 조준이 없다. 원점과 방향은 손 소켓에서 온다:
 * 손에서 나가고, 몸 중심에서 손으로 뻗은 쪽으로 날아가므로 애니메이션이 도는 대로 탄이 나간다.
 * 소켓이 없으면 예전 방식(캐릭터 중심의 손 높이 + 계산한 요)으로 돌아간다.
 *
 * 언제 쏘는지도 애니메이션이 정한다. 시퀀스에 얹은 Spinner Volley Window 구간 동안만 쏘므로,
 * 준비 동작이나 마무리 동안에는 나가지 않는다. 구간은 에셋에 박힌 데이터라 메시의 포즈를
 * 돌리지 않는 데디케이티드 서버에서도 그대로 읽힌다(런타임 노티파이로는 그게 안 된다).
 *
 * 클라이언트는 슬롯 컴포넌트의 멀티캐스트로 같은 산탄을 연출로 본다. 잉크는 쓰지 않는다.
 *
 * 끝 동작은 효과 밖의 마무리다(UItemAbility의 recovery). 회전이 끝나면 상태 태그가 내려가 총과
 * 다른 아이템이 풀리고, 끝 동작 중에 쏘거나 다른 아이템을 쓰면 끝 동작이 그 자리에서 끊긴다.
 */
UCLASS()
class MINTCHOCO_API UGA_SweetSpinner : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_SweetSpinner();

	/**
	 * 이번 발의 원점과 수평 방향. 손 소켓이 있으면 거기서 나가고, 방향도 몸 중심에서 손으로
	 * 뻗은 쪽이다 — 원점과 방향이 모두 애니메이션에서 오므로 도는 모습과 정확히 맞는다.
	 *
	 * 소켓이 없거나 손이 몸 중심 바로 위아래에 있으면 거짓이고, 그때는 호출부가 계산한 요로 쏜다.
	 */
	static bool ComputeHandMuzzle(const AUnit& Unit, const USweetSpinnerProfile& Profile, FVector& OutOrigin, FVector& OutFlatDirection);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override;

	/** 끝 동작이 있으면 회전이 끝나는 시각까지만 태그를 건다. */
	virtual float GetEffectDuration(const UItemProfile& Profile) const override;

	/** 끝 동작 한 번(지속시간 안으로 잘린다). */
	virtual float GetRecoveryDuration(const UItemProfile& Profile) const override;

	/** 끝 동작을 상체에 얹는다. */
	virtual void OnRecoveryStarted(AUnit& Unit, const UItemProfile& Profile) override;

private:
	/**
	 * 시작(상체) → 회전(전신)으로 자세를 넘길 타이머를 건다. 구간의 길이는 각 클립의 길이가 정한다.
	 * 끝(상체)은 여기서 예약하지 않고 마무리가 시작될 때(OnRecoveryStarted) 얹는다. 서버와 소유
	 * 클라이언트가 각자 굴리고, 나머지 머신은 슬롯의 복제로 따라온다.
	 */
	void SchedulePosePhases(AUnit& Unit);

	/** 자세를 갈아 끼운다. 슬롯이 없으면 아무것도 안 한다. */
	void SetPose(UAnimSequenceBase* Animation, EItemPoseBlend Blend);

	UFUNCTION()
	void EnterSpinPose();

	UFUNCTION()
	void EnterEndPose();

	/** 회전 구간이 시작될 때. 여기서부터 VolleyInterval 간격으로 산탄이 나간다. */
	UFUNCTION()
	void StartVolleys();

	UFUNCTION()
	void HandleVolley(int32 ActionNumber);

	UPROPERTY(Transient)
	TObjectPtr<const USweetSpinnerProfile> Spinner;

	/** 발동 순간 캐릭터가 보던 요. 첫 발이 여기서 나간다. */
	float StartYaw = 0.0f;
	int32 VolleyCount = 1;
};
