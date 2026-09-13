#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "HoneyBalloonAbility.generated.h"

class AItemAimPreview;
class UHoneyBalloonProfile;

/**
 * 꿀풍선. 아이템 키는 던지지 않고 조준을 시작한다: 던지는 본인의 화면에만 궤적이 그려지고,
 * 좌클릭이 던지며 우클릭이 물린다(물리면 아이템은 슬롯에 그대로 남는다 — 조준 중에는 슬롯을
 * 비우지 않기 때문이다).
 *
 * 던지는 것은 여전히 서버뿐이고, 나머지는 투사체의 일이다. 미리보기는 이 머신에만 있는
 * 표시라 아무것도 막지 않는다.
 */
UCLASS()
class MINTCHOCO_API UGA_HoneyBalloon : public UItemAbility
{
	GENERATED_BODY()

protected:
	virtual bool IsAimingItem() const override { return true; }

	virtual void OnAimStarted(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed) override;
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	void DestroyPreview();

	UPROPERTY(Transient)
	TObjectPtr<AItemAimPreview> Preview;

	UPROPERTY(Transient)
	TObjectPtr<const UHoneyBalloonProfile> Honey;
};
