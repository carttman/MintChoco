#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "BeeAbility.generated.h"

/** 꿀벌. 서버가 가장 가까운 상대를 골라 꿀벌을 날리고 끝난다. */
UCLASS()
class MINTCHOCO_API UGA_Bee : public UItemAbility
{
	GENERATED_BODY()

public:
	/** 사용자 본인과 같은 팀을 뺀 가장 가까운 유닛. 없으면 nullptr. */
	static AUnit* FindNearestOpponent(const AUnit& From);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
};
