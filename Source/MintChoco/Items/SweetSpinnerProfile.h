#pragma once

#include "CoreMinimal.h"

#include "Items/ItemProfile.h"

#include "SweetSpinnerProfile.generated.h"

class UAnimSequenceBase;
class UPaintGunProfile;

/** 스위트 스피너의 순수 계산. 테스트가 월드 없이 검사한다. */
namespace SweetSpinner
{
	/**
	 * Index번째 산탄의 요(도). 시작 요에서 출발해 Count발이 Turns바퀴를 고르게 나눠 돈다.
	 * 마지막 발은 한 걸음 못 미친 각이라 첫 발과 겹치지 않는다.
	 *
	 * 손 소켓을 찾지 못했을 때만 쓰인다. 평소에는 방향을 애니메이션의 손이 정한다.
	 */
	MINTCHOCO_API float VolleyYawDegrees(float StartYaw, int32 Index, int32 Count, float Turns);

	/**
	 * 시퀀스에서 회전 구간 표시(UAnimNotifyState_SpinnerVolley)를 찾아 시작·끝 시각(초)을 준다.
	 * 표시가 없거나 길이가 0이면 거짓이고, 그때는 지속시간 내내 쏜다.
	 */
	MINTCHOCO_API bool FindVolleyWindow(const UAnimSequenceBase* Sequence, float& OutStart, float& OutEnd);
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

	/**
	 * 탄이 나가는 손 소켓(또는 본). 애니메이션을 따라 도는 자리라, 원점과 방향이 모두 여기서 온다:
	 * 몸 중심에서 이 소켓으로 뻗은 쪽으로 날아가므로 회전과 정확히 맞는다.
	 *
	 * 비어 있거나 소켓이 없으면 예전 방식으로 돌아간다(캐릭터 중심의 손 높이 + Turns로 계산한 요).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner")
	FName HandSocket = TEXT("hand_r");

	/**
	 * 회전 구간을 표시해 둔 애니메이션. 그 시퀀스의 Spinner Volley Window 구간에서만 발사한다.
	 * 비어 있거나 표시가 없으면 지속시간 내내 쏜다.
	 *
	 * 캐릭터에 실제로 재생되는 것은 UseAnimation·PoseAnimation이다. 여기는 "언제 도는지"를 읽어
	 * 갈 곳을 가리킬 뿐이므로, 보통 같은 에셋을 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spinner")
	TObjectPtr<UAnimSequenceBase> SpinAnimation;

	/** 발사 구간(초). 표시가 없으면 [0, Duration] 전체다. */
	void GetVolleyWindow(float& OutStart, float& OutEnd) const;

	/** 주어진 구간을 VolleyInterval로 나눈 발사 횟수. 최소 1. */
	int32 GetVolleyCount(float Window) const;

	/** 지속시간 전체로 나눈 발사 횟수. 최소 1. */
	int32 GetVolleyCount() const;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
