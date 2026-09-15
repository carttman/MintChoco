#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "AnimNotify_Footstep.generated.h"

/**
 * 발소리. 달리기 시퀀스에서 발이 바닥에 닿는 프레임에 놓는다. 소켓(또는 본) 위치에서
 * Audio.Unit.Footstep을 울리고, 캐릭터별 소리는 UnitData의 Sounds 뱅크가 정한다.
 *
 * 애니메이션은 머신마다 각자 돌므로 소리도 각자 난다. 데디케이티드 서버는 서브시스템이 거른다.
 */
UCLASS(meta = (DisplayName = "Footstep (MintChoco)"))
class MINTCHOCO_API UAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 소리가 나는 소켓 또는 본. 없으면 메시 위치. */
	UPROPERTY(EditAnywhere, Category = "Footstep")
	FName FootSocket = TEXT("foot_l");
};
