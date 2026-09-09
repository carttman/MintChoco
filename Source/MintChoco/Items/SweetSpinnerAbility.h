#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SweetSpinnerAbility.generated.h"

class USweetSpinnerProfile;

/**
 * 스위트 스피너. 캐릭터를 요 축으로 빠르게 돌리며 서버가 일정 간격으로 산탄을 쏜다.
 *
 * 회전은 예측 클라이언트와 서버가 각자 돌린다. 컨트롤 요 추종을 잠시 끄고 액터를 직접
 * 돌리므로 카메라(컨트롤 회전을 따르는 붐)는 돌지 않는다. 소유자의 회전은 ServerMove에
 * 실리지 않지만 서버도 같은 속도로 돌리고, 다른 클라이언트는 서버의 회전을 복제로 본다.
 * 서버 보정(ClientAdjustPosition)은 클라이언트의 저장된 회전을 되돌리므로 로컬 회전과
 * 싸우지 않는다.
 *
 * 산탄은 서버만 쏜다. 무기의 ServerFire와 달리 조준이 없고 액터 정면이 곧 방향이다.
 * 클라이언트는 슬롯 컴포넌트의 멀티캐스트로 같은 산탄을 연출로 본다. 잉크는 쓰지 않는다.
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
	void HandleTick(float DeltaTime);

	UFUNCTION()
	void HandleVolley(int32 ActionNumber);

	UPROPERTY(Transient)
	TObjectPtr<const USweetSpinnerProfile> Spinner;
};
