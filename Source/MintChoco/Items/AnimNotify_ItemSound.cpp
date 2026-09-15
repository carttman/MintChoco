#include "Items/AnimNotify_ItemSound.h"

#include "Components/SkeletalMeshComponent.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Game/Unit.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"

UAnimNotify_ItemSound::UAnimNotify_ItemSound()
{
	Tag = AudioTags::Audio_Item_Activate;
}

void UAnimNotify_ItemSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || !Tag.IsValid())
	{
		return;
	}

	// 아이템이 없으면(에디터 미리보기, 효과가 끝난 뒤 남은 클립) 기본 뱅크로 내려간다.
	const AUnit* const Unit = Cast<AUnit>(MeshComp->GetOwner());
	const UItemSlotComponent* const Slot = Unit ? Unit->GetItemSlot() : nullptr;
	const UItemProfile* const Item = Slot ? Slot->GetAnimationItem() : nullptr;

	const FName AttachSocket = Socket != NAME_None && MeshComp->DoesSocketExist(Socket) ? Socket : NAME_None;
	UGameAudioSubsystem::PlayAttached(Tag, MeshComp, AttachSocket, Item ? Item->Sounds.Get() : nullptr);
}

FString UAnimNotify_ItemSound::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? FString::Printf(TEXT("Item Sound: %s"), *Tag.ToString()) : TEXT("Item Sound");
}
