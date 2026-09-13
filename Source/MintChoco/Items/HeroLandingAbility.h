#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "HeroLandingAbility.generated.h"

class UHeroLandingProfile;

/**
 * 히어로 랜딩. 이 클래스는 움직임을 만지지 않는다. 무브먼트 컴포넌트에 튜닝값과 의도
 * (압축 플래그)를 넣고, 착지 알림을 기다렸다가 서버에서 효과를 낸다. 소유 클라이언트는
 * 그동안 착지점 표시를 조준점에 따라 옮긴다.
 */
UCLASS()
class MINTCHOCO_API UGA_HeroLanding : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_HeroLanding();

	/** 상승·정지 중의 좌클릭. 그 순간 내리꽂기를 시작한다. */
	virtual bool WantsInput(EItemAbilityInput Input) const override;
	virtual void HandleInput(EItemAbilityInput Input) override;

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override;

private:
	void HandleLanded();

	UFUNCTION()
	void HandleTick(float DeltaTime);

	void DestroyMarker();

	UPROPERTY(Transient)
	TObjectPtr<const UHeroLandingProfile> Landing;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Marker;

	FDelegateHandle LandedHandle;
};
