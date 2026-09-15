#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"

#include "AnimNotifyState_ItemSound.generated.h"

/**
 * 아이템 소리를 애니메이션의 구간 동안만 낸다. 구간 시작에 붙여서 틀고 끝에 페이드로 끈다.
 * 클립이 다른 자세로 갈아타거나 블렌드 아웃돼 구간이 중간에 끊겨도 애님 인스턴스가 NotifyEnd를
 * 불러 주므로 그때도 꺼진다. 끝까지 들려야 하는 소리는 UAnimNotify_ItemSound를 쓴다.
 *
 * 노티파이 객체는 모든 캐릭터가 공유하는 에셋이라 상태를 두지 않는다. 오디오 컴포넌트는
 * 유닛의 아이템 슬롯(UItemSlotComponent::PlayAnimationSound / StopAnimationSound)이 태그별로 든다.
 * 어떤 아이템의 뱅크를 쓸지도 슬롯이 정한다(GetAnimationItem).
 */
UCLASS(meta = (DisplayName = "Item Sound Range (MintChoco)"))
class MINTCHOCO_API UAnimNotifyState_ItemSound : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_ItemSound();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 울릴 이벤트. 기본은 발동음(Audio.Item.Activate). */
	UPROPERTY(EditAnywhere, Category = "Item Sound", meta = (Categories = "Audio.Item"))
	FGameplayTag Tag;

	/** 소리가 붙을 소켓 또는 본. 없으면 메시 루트. */
	UPROPERTY(EditAnywhere, Category = "Item Sound")
	FName Socket;

	/** 구간이 끝날 때 줄어드는 시간(초). 0이면 뚝 끊는다. */
	UPROPERTY(EditAnywhere, Category = "Item Sound", meta = (ClampMin = "0", ForceUnits = "s"))
	float FadeOut = 0.1f;
};
