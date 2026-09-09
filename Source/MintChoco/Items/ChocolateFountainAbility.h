#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "ChocolateFountainAbility.generated.h"

/** 초콜릿 분수. 서버가 사용자 발밑에 돔을 세우고 끝난다. */
UCLASS()
class MINTCHOCO_API UGA_ChocolateFountain : public UItemAbility
{
	GENERATED_BODY()

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
};
