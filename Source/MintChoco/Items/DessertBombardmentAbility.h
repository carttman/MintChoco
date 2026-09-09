#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "DessertBombardmentAbility.generated.h"

class UWorld;

/** 디저트 폭격. 서버가 맵 범위를 재고 APaintRain을 스폰하고 끝난다. */
UCLASS()
class MINTCHOCO_API UGA_DessertBombardment : public UItemAbility
{
	GENERATED_BODY()

public:
	/** 위를 향한 면이 있는 도색 가능 표면들의 월드 경계 합집합. 하나도 없으면 무효 상자. */
	static FBox ComputeMapBounds(const UWorld& World);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
};
