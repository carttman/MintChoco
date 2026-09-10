#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProfile.h"

#include "SweetSpinnerProfile.generated.h"

class UPaintGunProfile;

/** 스위트 스피너의 순수 계산. 테스트가 월드 없이 검사한다. */
namespace SweetSpinner
{
	/**
	 * Index번째 산탄의 요(도). 시작 요에서 출발해 Count발이 Turns바퀴를 고르게 나눠 돈다.
	 * 마지막 발은 한 걸음 못 미친 각이라 첫 발과 겹치지 않는다.
	 */
	MINTCHOCO_API float VolleyYawDegrees(float StartYaw, int32 Index, int32 Count, float Turns);
}

/**
 * 스위트 스피너: 캐릭터는 그대로 서 있고, 산탄의 발사 방향만 제자리에서 몇 바퀴 돌며 사방에 뿌린다.
 * 캐릭터가 도는 모습은 애니메이션이 맡는다. 몇 바퀴를 몇 발로 나눌지는 여기, 산탄 한 발이
 * 무엇인지는 무기 프로필(Volley)이 정한다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API USweetSpinnerProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	/** 지속시간 동안 발사 방향이 도는 바퀴 수. 3이면 1080도. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner", meta = (ClampMin = "0"))
	float Turns = 3.0f;

	/** 산탄 사이의 간격(초). Duration / VolleyInterval 번 발사한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float VolleyInterval = 0.1f;

	/**
	 * 발사 방향을 위로 들어 올리는 각(도). 0이면 수평, 양수면 위로 뿌린다. 탄은 중력을 받으므로
	 * 각을 올릴수록 멀리 포물선으로 날아가다 떨어진다. 음수면 발밑을 향해 뿌린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float PitchDeg = 15.0f;

	/** 한 번의 산탄. 총 프로필이라 알약·산탄 패턴·사거리(총구 속도와 중력)를 그대로 재사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner")
	TObjectPtr<UPaintGunProfile> Volley;

	/** 지속시간과 간격으로 정해지는 발사 횟수. 최소 1. */
	int32 GetVolleyCount() const;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
