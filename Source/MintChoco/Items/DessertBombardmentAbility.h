#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "DessertBombardmentAbility.generated.h"

class AActor;
class UDessertBombardmentProfile;
class UWorld;

/**
 * 디저트 폭격. 아이템 키는 쏘지 않고 조준을 시작한다: 바라보는 수평 방향으로 발사 경로가
 * 보이고, 좌클릭이 그 경로를 따라 APaintRain을 스폰하며 우클릭이 물린다(물리면 조준 중에는
 * 슬롯을 비우지 않으므로 아이템이 그대로 남는다).
 *
 * 스폰은 서버뿐이고 경로 표시는 조준하는 본인의 화면에만 있다.
 */
UCLASS()
class MINTCHOCO_API UGA_DessertBombardment : public UItemAbility
{
	GENERATED_BODY()

public:
	/** 위를 향한 면이 있는 도색 가능 표면들의 월드 경계 합집합. 하나도 없으면 무효 상자. */
	static FBox ComputeMapBounds(const UWorld& World);

protected:
	virtual bool IsAimingItem() const override { return true; }

	virtual void OnAimStarted(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed) override;
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	/**
	 * 미리보기 박스의 트랜스폼. 실제로 떨어질 행 수를 그대로 써서 1×1×1 큐브를
	 * 폭격 가능 길이 × 열 폭으로 늘인다. 보이는 것과 떨어지는 것이 같아진다.
	 */
	FTransform ComputePreviewTransform(const AUnit& Unit, const UDessertBombardmentProfile& Profile) const;

	void DestroyPreview();

	UPROPERTY(Transient)
	TObjectPtr<AActor> Preview;

	UPROPERTY(Transient)
	TObjectPtr<const UDessertBombardmentProfile> Bombardment;

	/** 조준 시작에 한 번 재 둔다. 매 프레임 모든 도색 표면을 다시 훑을 이유가 없다. */
	FBox CachedBounds = FBox(ForceInit);
};
