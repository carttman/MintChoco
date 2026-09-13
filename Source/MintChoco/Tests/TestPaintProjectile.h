#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintProjectile.h"

#include "TestPaintProjectile.generated.h"

/**
 * 자동화 테스트가 스폰하는 구체 페인트볼. APaintProjectile은 Abstract이라 직접 스폰할 수
 * 없고, 게임의 BP 페인트볼은 메시와 팀 재질을 들고 있어 테스트가 에셋에 의존하게 된다.
 * 여기는 아무것도 더하지 않는다 — 풀의 재사용 동작만 보면 되기 때문이다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API ATestPaintProjectile : public APaintProjectile
{
	GENERATED_BODY()
};
