#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProjectile.h"

#include "HoneyBalloonProjectile.generated.h"

class UHoneyBalloonProfile;

/**
 * 던져진 꿀풍선. 지형이든 다른 플레이어든 닿는 순간 터진다. 직격과 바닥의 차이는 없다:
 * 터진 자리에서 탄을 뿌리고 반경 안의 상대를 스턴한다.
 */
UCLASS()
class MINTCHOCO_API AHoneyBalloonProjectile : public AItemProjectile
{
	GENERATED_BODY()

public:
	/** 서버 전용. Init 뒤에. 터질 때 무엇을 뿌릴지. */
	void SetProfile(const UHoneyBalloonProfile* InProfile);

protected:
	virtual void HandleUnitOverlap(AUnit& Unit) override;
	virtual void OnDetonate() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UHoneyBalloonProfile> Profile;
};
