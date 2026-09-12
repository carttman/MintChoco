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

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override;

private:
	void HandleLanded();

	UFUNCTION()
	void HandleTick(float DeltaTime);

	void DestroyMarker();

	/**
	 * 카메라 붐을 올리거나 되돌린다. 소유 클라이언트에서만 부른다: 카메라는 그 머신의
	 * 것이라 복제할 것이 없다.
	 */
	void SetCameraRaised(AUnit& Unit, bool bRaise);

	UPROPERTY(Transient)
	TObjectPtr<const UHeroLandingProfile> Landing;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Marker;

	FDelegateHandle LandedHandle;

	/** 올리기 전의 붐 오프셋. 되돌릴 때 이 값을 그대로 쓴다. */
	FVector SavedBoomOffset = FVector::ZeroVector;

	bool bCameraRaised = false;

	/** 호버 이펙트를 이미 뿌렸는지. 서버에서만 본다. 호버는 한 번뿐이라 한 번만 보낸다. */
	bool bHoverFXSent = false;
};
