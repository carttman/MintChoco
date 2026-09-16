#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "DessertBombardmentAbility.generated.h"

class UDessertBombardmentProfile;
class UNiagaraComponent;
class UWorld;

/**
 * 미리보기가 그릴 띠. 실제로 떨어질 행 수를 그대로 쓰므로 보이는 자리와 떨어지는 자리가 같다.
 *
 * Transform은 띠의 중심(발밑 높이, 수평 시선 방향)이고, 크기는 스케일이 아니라 따로 나간다 —
 * 이펙트는 유저 파라미터로 받아 스스로 그 길이에 맞춘다.
 */
struct FBombardmentPreviewShape
{
	FTransform Transform = FTransform::Identity;

	/** 발밑에서 앞으로 뻗는 길이(cm). */
	float Length = 0.0f;

	/** 열 폭(cm). */
	float Width = 0.0f;
};

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

	FBombardmentPreviewShape ComputePreviewShape(const AUnit& Unit, const UDessertBombardmentProfile& Profile) const;

	/** 띠를 이펙트에 옮긴다: 위치와 방향은 컴포넌트가, 길이와 폭은 유저 파라미터가 받는다. */
	void ApplyPreviewShape(const FBombardmentPreviewShape& Shape);

	/** mc.Bombardment.PreviewDebug 가 1이면 코드가 잡은 띠를 선으로 그린다. 이펙트와 비교하는 용도. */
	void DrawPreviewDebug(const FBombardmentPreviewShape& Shape) const;

	void DestroyPreview();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Preview;

	UPROPERTY(Transient)
	TObjectPtr<const UDessertBombardmentProfile> Bombardment;

	/** 조준 시작에 한 번 재 둔다. 매 프레임 모든 도색 표면을 다시 훑을 이유가 없다. */
	FBox CachedBounds = FBox(ForceInit);
};
