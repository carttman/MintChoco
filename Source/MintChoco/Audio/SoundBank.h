#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "SoundBank.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * 사운드 하나와 그것을 어떻게 틀지. 재생 쪽 코드가 알아야 할 것은 전부 여기 들어 있어서,
 * 호출부는 태그만 넘기고 볼륨·피치·감쇠는 신경 쓰지 않는다.
 */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FSoundEvent
{
	GENERATED_BODY()

	/** 실제로 나갈 소리. 비어 있으면 이 이벤트는 조용히 넘어간다(연출 구멍이지 오류가 아니다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	/**
	 * 거리 감쇠. 비어 있으면 설정의 기본 감쇠를 쓴다. b2D가 켜져 있으면 무시된다.
	 * 소리마다 들려야 하는 거리가 다르므로(발소리와 폭발) 이벤트 단위로 갈아끼운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	/**
	 * 동시 발성 제한. 한 프레임에 수십 개가 날 수 있는 소리(스플랫, 페인트볼 충돌)에는
	 * 반드시 채워야 한다. 비어 있으면 제한 없이 울린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundConcurrency> Concurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	/**
	 * 재생마다 뽑는 피치 범위. 같은 소리가 연달아 날 때(발사, 발소리) 기계음처럼 들리지
	 * 않게 한다. 두 값이 같으면 랜덤 없이 그 값으로 고정된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1"))
	FVector2D PitchRange = FVector2D(1.0f, 1.0f);

	/**
	 * 위치 없이 2D로 재생한다. UI와 음악, 그리고 "내 화면에만 들려야 하는" 피드백
	 * (잉크 부족 거절음)이 여기 해당한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool b2D = false;
};

/**
 * 이벤트 태그 → 소리 표. 게임의 소리는 전부 이 표를 거치므로, 무엇이 어떤 소리를 내는지
 * 한 에셋에서 보고 갈아끼울 수 있다.
 *
 * 프로젝트 기본 뱅크는 UGameAudioSettings가 가리키는 하나이고, 캐릭터나 무기처럼 개체마다
 * 달라야 하는 소리는 그 데이터 에셋이 작은 오버라이드 뱅크를 하나 더 들고 있다.
 * 조회는 오버라이드 → 기본 순이라, 오버라이드에는 바꿀 항목만 넣으면 된다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API USoundBank : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 등록되지 않은 태그면 nullptr. */
	const FSoundEvent* Find(const FGameplayTag& Tag) const;

	/** 표에 들어 있는 모든 태그. 테스트가 빈 슬롯을 훑을 때 쓴다. */
	void GetTags(TArray<FGameplayTag>& OutTags) const { Events.GetKeys(OutTags); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ForceInlineRow, Categories = "Audio"))
	TMap<FGameplayTag, FSoundEvent> Events;
};
