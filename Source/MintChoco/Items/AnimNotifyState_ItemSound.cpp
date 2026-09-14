#include "Items/AnimNotifyState_ItemSound.h"

#include "Components/SkeletalMeshComponent.h"

#include "Audio/AudioGameplayTags.h"
#include "Game/Unit.h"
#include "Items/ItemSlotComponent.h"

namespace
{
	UItemSlotComponent* FindSlot(const USkeletalMeshComponent* MeshComp)
	{
		const AUnit* const Unit = MeshComp ? Cast<AUnit>(MeshComp->GetOwner()) : nullptr;
		return Unit ? Unit->GetItemSlot() : nullptr;
	}
}

UAnimNotifyState_ItemSound::UAnimNotifyState_ItemSound()
{
	Tag = AudioTags::Audio_Item_Activate;
}

void UAnimNotifyState_ItemSound::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UItemSlotComponent* const Slot = FindSlot(MeshComp))
	{
		Slot->PlayAnimationSound(Tag, Socket);
	}
}

void UAnimNotifyState_ItemSound::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UItemSlotComponent* const Slot = FindSlot(MeshComp))
	{
		Slot->StopAnimationSound(Tag, FadeOut);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_ItemSound::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? FString::Printf(TEXT("Item Sound Range: %s"), *Tag.ToString()) : TEXT("Item Sound Range");
}
