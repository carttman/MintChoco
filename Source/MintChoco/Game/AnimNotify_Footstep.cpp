#include "Game/AnimNotify_Footstep.h"

#include "Components/SkeletalMeshComponent.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Game/Unit.h"
#include "Game/UnitDataAsset.h"

void UAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp)
	{
		return;
	}

	const FVector Location = FootSocket != NAME_None && MeshComp->DoesSocketExist(FootSocket)
		? MeshComp->GetSocketLocation(FootSocket)
		: MeshComp->GetComponentLocation();

	const AUnit* const Unit = Cast<AUnit>(MeshComp->GetOwner());
	const UUnitDataAsset* const UnitData = Unit ? Unit->GetUnitData() : nullptr;
	UGameAudioSubsystem::PlayAt(MeshComp, AudioTags::Audio_Unit_Footstep, Location, UnitData ? UnitData->Sounds.Get() : nullptr);
}

FString UAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return TEXT("Footstep");
}
