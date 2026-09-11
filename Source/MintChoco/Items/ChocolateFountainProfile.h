#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"

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

	/**
	 * 돔이 설 때 발밑에서 한 번 뿌리는 탄. 자기 팀 색이라 돔 벽을 그대로 통과한다.
	 * PaintId와 Seed는 런타임에 채워진다. Paintball이 비어 있으면 아무것도 뿌리지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain")
	FPaintBurstParams Burst;

	/**
	 * 참이면 Burst.Speed를 무시하고 Radius만큼 닿는 속도를 계산해 쓴다. 반경을 바꾸면 도포 범위가
	 * 따라온다. 거짓이면 Burst.Speed를 그대로 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain")
	bool bBurstMatchesRadius = true;

	/** 이번 도포에 쓸 파라미터. 색과 시드를 채우고, 필요하면 속도를 반경에 맞춘다. */
	FPaintBurstParams MakeBurst(uint8 InPaintId, int32 Seed) const;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
