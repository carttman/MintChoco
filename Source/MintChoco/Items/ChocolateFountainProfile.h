#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"

#include "ChocolateFountainProfile.generated.h"

class AChocolateFountain;

/**
 * 초콜릿 분수: 사용자 발밑에 갈색 반구를 세운다. 반구는 상대 페인트탄을 삼키고 상대 폰을
 * 막으며, 생성 순간 안에 있던 상대는 밖으로 튕겨낸다. 아군은 자유롭게 드나든다. 즉발(Duration 0);
 * 돔 자체가 Lifetime 동안 산다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UChocolateFountainProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain")
	TSubclassOf<AChocolateFountain> DomeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain", meta = (ClampMin = "50", ForceUnits = "cm"))
	float Radius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain", meta = (ClampMin = "0.5", ForceUnits = "s"))
	float Lifetime = 5.0f;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
