#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAimAbility.h"

#include "DessertBombardmentAbility.generated.h"

class UDessertBombardmentProfile;
class UWorld;

/**
 * 디저트 폭격. 아이템을 쓰면 바라보는 수평 방향으로 발사 경로가 보이고,
 * 좌클릭하면 그 경로를 따라 서버가 APaintRain을 스폰한다.
 *
 * 조준 제한 시간은 프로필의 Duration(10초)이다. 그 안에 쓰지 않으면 효과가 만료되고
 * 아이템은 그대로 잃는다 — 취소 입력은 없다.
 */
UCLASS()
class MINTCHOCO_API UGA_DessertBombardment : public UItemAimAbility
{
	GENERATED_BODY()

public:
	UGA_DessertBombardment();

	/** 위를 향한 면이 있는 도색 가능 표면들의 월드 경계 합집합. 하나도 없으면 무효 상자. */
	static FBox ComputeMapBounds(const UWorld& World);

protected:
	virtual void OnAimBegan(AUnit& Unit, const UItemProfile& Profile) override;
	virtual AActor* SpawnPreview(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void UpdatePreview(AUnit& Unit, AActor& InPreview, float DeltaTime) override;
	virtual void OnAimConfirmed(AUnit& Unit, const UItemProfile& Profile) override;

private:
	/**
	 * 미리보기 박스의 트랜스폼. 실제로 떨어질 행 수를 그대로 써서 1×1×1 큐브를
	 * 폭격 가능 길이 × 열 폭으로 늘인다. 보이는 것과 떨어지는 것이 같아진다.
	 */
	FTransform ComputePreviewTransform(const AUnit& Unit, const UDessertBombardmentProfile& Bombardment) const;

	/** 조준 시작에 한 번 재 둔다. 매 프레임 모든 도색 표면을 다시 훑을 이유가 없다. */
	FBox CachedBounds = FBox(ForceInit);
};
