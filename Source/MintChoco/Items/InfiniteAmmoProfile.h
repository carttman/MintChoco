#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProfile.h"

#include "InfiniteAmmoProfile.generated.h"

/**
 * 무한 탄환: Duration 동안 페인트탄을 소모하지 않는다. 무기가 효과 태그를 보고 비용을 0으로
 * 치고, 잉크병은 오버라이드 재질(빨강)로 바뀌었다가 마지막 1초에 점멸한다. 값은 프로필에
 * 더 없다: 지속시간이 곧 전부다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UInfiniteAmmoProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	virtual bool OverridesInkLook() const override { return true; }
};
