#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "InfiniteAmmoAbility.generated.h"

/**
 * 무한 탄환. 효과는 전부 태그가 낸다(무기 비용 0, 잉크병 재질). 여기서는 재발동 때 잉크병의
 * 점멸 타이머를 다시 시작하는 일만 한다: 갱신은 태그 수가 1에서 1로 머물러 슬롯이 모른다.
 */
UCLASS()
class MINTCHOCO_API UGA_InfiniteAmmo : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_InfiniteAmmo();

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
};
