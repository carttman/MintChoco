#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AnimNotify_ItemSound.generated.h"

/**
 * 아이템 소리를 애니메이션의 특정 프레임에서 낸다. 사용 동작이나 자세 시퀀스(스위트 스피너의
 * SP_Start/SP_Loop/SP_End)에 놓는다.
 *
 * 어떤 아이템의 소리인지는 노티파이가 아니라 슬롯이 안다: 메시 주인 유닛의 아이템 슬롯에서
 * 지금 애니메이션을 정하는 아이템(UItemSlotComponent::GetAnimationItem)을 찾아 그 프로필의
 * Sounds 뱅크로 태그를 울린다. 그래서 같은 노티파이를 어느 아이템 클립에나 놓을 수 있다.
 *
 * 효과 시작 순간의 발동음과 겹치지 않으려면 프로필의 bActivateSoundFromAnimation을 켠다.
 * 애니메이션은 머신마다 각자 돌므로 소리도 각자 난다. 데디케이티드 서버는 서브시스템이 거른다.
 */
UCLASS(meta = (DisplayName = "Item Sound (MintChoco)"))
class MINTCHOCO_API UAnimNotify_ItemSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ItemSound();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 울릴 이벤트. 기본은 발동음(Audio.Item.Activate). */
	UPROPERTY(EditAnywhere, Category = "Item Sound", meta = (Categories = "Audio.Item"))
	FGameplayTag Tag;

	/** 소리가 붙을 소켓 또는 본. 없으면 메시 루트. */
	UPROPERTY(EditAnywhere, Category = "Item Sound")
	FName Socket;
};
