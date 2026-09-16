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
	float Lifetime = 3.0f;

	/**
	 * 발밑 도포를 몇 번 하는가. 설치 순간의 첫 번째를 포함한다. 1이면 예전처럼 한 번만 뿌린다.
	 *
	 * 마지막 도포와 돔이 같이 끝나야 보기에 맞으므로 (BurstCount - 1) × BurstInterval을
	 * Lifetime에 맞춰 둔다. 넘으면 남은 도포가 수명 안으로 당겨지고, 그래도 남으면 잘린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain", meta = (ClampMin = "1"))
	int32 BurstCount = 4;

	/** 도포 사이의 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float BurstInterval = 1.0f;

	/**
	 * 도포 한 번마다 반경에 곱해지는 배율. 1.4면 1.0 → 1.4 → 1.96 → 2.74배로 넓어진다.
	 * 1이면 매번 같은 범위. **돔 크기는 따라 커지지 않는다** — 커지는 것은 바닥 도포뿐이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain", meta = (ClampMin = "0.1"))
	float BurstGrowth = 1.4f;

	/**
	 * 돔이 설 때 발밑에서 뿌리는 탄. 자기 팀 색이라 돔 벽을 그대로 통과한다.
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

	/**
	 * 이번 도포에 쓸 파라미터. 색과 시드를 채우고, 필요하면 속도를 반경에 맞춘다.
	 * RadiusScale은 Radius에 곱해지는 배율이다(넓어지는 도포용). 1이면 돔과 같은 범위.
	 */
	FPaintBurstParams MakeBurst(uint8 InPaintId, int32 Seed, float RadiusScale = 1.0f) const;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
