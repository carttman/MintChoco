#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SweetSpinnerAbility.generated.h"

class USweetSpinnerProfile;

/**
 * 스위트 스피너. 캐릭터는 돌지 않고, 서버가 일정 간격으로 쏘는 산탄의 방향만 시작 요에서
 * 출발해 Turns바퀴를 고르게 나눠 돈다(SweetSpinner::VolleyYawDegrees). 캐릭터가 도는 모습은
 * 애니메이션이 맡으므로 액터 회전과 컨트롤 요 추종은 건드리지 않는다.
 *
 * 산탄은 서버만 쏜다. 무기의 ServerFire와 달리 조준이 없고, 발사 원점은 캐릭터 중심(손 높이),
 * 방향은 그 발의 요다. 클라이언트는 슬롯 컴포넌트의 멀티캐스트로 같은 산탄을 연출로 본다.
 * 잉크는 쓰지 않는다.
 */
UCLASS()
class MINTCHOCO_API UGA_SweetSpinner : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_SweetSpinner();

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override;

private:
	UFUNCTION()
	void HandleVolley(int32 ActionNumber);

	UPROPERTY(Transient)
	TObjectPtr<const USweetSpinnerProfile> Spinner;

	/** 발동 순간 캐릭터가 보던 요. 첫 발이 여기서 나간다. */
	float StartYaw = 0.0f;
	int32 VolleyCount = 1;
};
