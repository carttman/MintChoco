#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "HoneyBalloonAbility.generated.h"

/** 꿀풍선. 서버가 시선 방향으로 투사체 하나를 던지고 끝난다. 나머지는 투사체의 일이다. */
UCLASS()
class MINTCHOCO_API UGA_HoneyBalloon : public UItemAbility
{
	GENERATED_BODY()

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
};
