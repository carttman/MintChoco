#include "Items/ItemAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"
#include "MintChoco.h"

UItemAbility::UItemAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 효과 중 같은 아이템을 다시 쓰면 이 인스턴스를 끝내고 다시 시작한다. 두 인스턴스가
	// 동시에 도는 대신 GE 스택이 타이머를 갱신하므로 "다시 시작"이 된다.
	bRetriggerInstancedAbility = true;
}

AUnit* UItemAbility::GetUnit() const
{
	return Cast<AUnit>(GetAvatarActorFromActorInfo());
}

const UItemProfile* UItemAbility::GetItemProfile() const
{
	return Cast<UItemProfile>(GetCurrentSourceObject());
}

bool UItemAbility::IsAuthority() const
{
	const FGameplayAbilityActivationInfo Info = GetCurrentActivationInfo();
	return HasAuthority(&Info);
}

void UItemAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	AUnit* const Unit = GetUnit();
	const UItemProfile* const Profile = GetItemProfile();
	if (!Unit || !Profile || !EffectClass)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: item ability activated without a unit, a profile or an effect class (%s / %s / %s)."),
			*GetName(), *GetNameSafe(Unit), *GetNameSafe(Profile), *GetNameSafe(EffectClass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 슬롯은 서버가 비운다. 소유자의 HUD는 복제로 따라온다.
	if (HasAuthority(&ActivationInfo))
	{
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->ConsumeHeldItem();
		}
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
	if (!Spec.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ItemTags::Data_Item_Duration, Profile->Duration);
	Spec.Data->DynamicGrantedTags.AddTag(StateTag);
	Spec.Data->GetContext().AddSourceObject(Profile);
	const FActiveGameplayEffectHandle EffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);

	if (HasAuthority(&ActivationInfo))
	{
		if (EffectHandle.IsValid())
		{
			UAbilityTask_WaitGameplayEffectRemoved* const Wait = UAbilityTask_WaitGameplayEffectRemoved::WaitForGameplayEffectRemoved(this, EffectHandle);
			Wait->OnRemoved.AddDynamic(this, &UItemAbility::HandleEffectRemoved);
			Wait->InvalidHandle.AddDynamic(this, &UItemAbility::HandleEffectRemoved);
			Wait->ReadyForActivation();
		}
		else
		{
			// 스펙이 거부된 경우(면역 등). 지금은 그런 GE가 없으므로 방어 코드다.
			UAbilityTask_WaitDelay* const Delay = UAbilityTask_WaitDelay::WaitDelay(this, Profile->Duration);
			Delay->OnFinish.AddDynamic(this, &UItemAbility::HandleDurationElapsed);
			Delay->ReadyForActivation();
		}
	}
	else
	{
		UAbilityTask_WaitDelay* const Delay = UAbilityTask_WaitDelay::WaitDelay(this, Profile->Duration);
		Delay->OnFinish.AddDynamic(this, &UItemAbility::HandleDurationElapsed);
		Delay->ReadyForActivation();
	}

	bItemStarted = true;
	OnItemActivated(*Unit, *Profile);
}

void UItemAbility::HandleEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	EndFromTimer();
}

void UItemAbility::HandleDurationElapsed()
{
	EndFromTimer();
}

void UItemAbility::EndFromTimer()
{
	if (IsActive())
	{
		// 서버의 종료는 ClientEndAbility로 클라이언트에도 닿는다. 클라이언트의 로컬 종료는
		// 자기만의 것이라 복제하지 않는다; 서버가 자기 시계로 따로 끝낸다.
		const bool bReplicate = IsAuthority();
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), bReplicate, /*bWasCancelled=*/false);
	}
}

void UItemAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bItemStarted)
	{
		bItemStarted = false;
		AUnit* const Unit = GetUnit();
		const UItemProfile* const Profile = GetItemProfile();
		if (Unit && Profile)
		{
			OnItemEnded(*Unit, *Profile);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
