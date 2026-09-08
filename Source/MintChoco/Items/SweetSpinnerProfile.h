#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProfile.h"

#include "SweetSpinnerProfile.generated.h"

class UPaintGunProfile;

/**
 * 스위트 스피너: 캐릭터가 제자리에서 빠르게 돌며 산탄을 사방에 뿌린다.
 * 회전 속도와 산탄 간격은 여기, 산탄 한 발이 무엇인지는 무기 프로필(Volley)이 정한다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API USweetSpinnerProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	/** 초당 회전각. 720이면 1초에 두 바퀴. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float SpinRateDeg = 720.0f;

	/** 산탄 사이의 간격(초). Duration / VolleyInterval 번 발사한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float VolleyInterval = 0.1f;

	/** 한 번의 산탄. 총 프로필이라 알약·산탄 패턴·사거리(총구 속도와 중력)를 그대로 재사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner")
	TObjectPtr<UPaintGunProfile> Volley;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
