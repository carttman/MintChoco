#include "Items/ItemAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include "Game/TeamTypes.h"
#include "Game/Unit.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"
#include "MintChoco.h"
#include "Weapons/PaintWeaponComponent.h"

UItemAbility::UItemAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 효과 중 같은 아이템을 다시 쓰면 이 인스턴스를 끝내고 다시 시작한다. 두 인스턴스가
	// 동시에 도는 대신 GE 스택이 타이머를 갱신하므로 "다시 시작"이 된다.
	bRetriggerInstancedAbility = true;

	// 스턴 중에는 아이템을 쓸 수 없다. 이미 도는 효과는 끊지 않는다.
	ActivationBlockedTags.AddTag(ItemTags::State_Status_Stunned);
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

uint8 UItemAbility::GetPaintId() const
{
	const AUnit* const Unit = GetUnit();
	if (!Unit)
	{
		return 0;
	}
	if (Teams::IsValidId(Unit->GetTeam()))
	{
		return static_cast<uint8>(Unit->GetTeam());
	}
	return Unit->GetPaintWeapon() ? Unit->GetPaintWeapon()->GetPaintId() : 0;
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
	if (!Unit || !Profile || (!Profile->IsInstant() && !EffectClass))
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: item ability activated without a unit, a profile or an effect class (%s / %s / %s)."),
			*GetName(), *GetNameSafe(Unit), *GetNameSafe(Profile), *GetNameSafe(EffectClass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 조준형: 아직 아무것도 일어나지 않는다. 슬롯도 그대로, GE도 없다. 좌클릭이 오면 ConfirmAim이
	// 아래의 StartItem을 부르고, 우클릭이 오면 손대지 않은 채로 끝난다.
	if (IsAimingItem())
	{
		bAiming = true;

		// 조준은 태그를 남기지 않으므로 다른 머신에는 이 플래그가 유일한 신호다(조준 자세용).
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->SetAiming(true);
		}

		OnAimStarted(*Unit, *Profile);
		return;
	}

	StartItem();
}

void UItemAbility::StartItem()
{
	AUnit* const Unit = GetUnit();
	const UItemProfile* const Profile = GetItemProfile();
	if (!Unit || !Profile)
	{
		return;
	}

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	// 슬롯은 서버가 비운다. 소유자의 HUD는 복제로 따라온다.
	if (HasAuthority(&ActivationInfo))
	{
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->ConsumeHeldItem();
		}
	}

	AppliedEffect = FActiveGameplayEffectHandle();

	// 즉발: GE 없이 효과를 내고 바로 끝난다. 남는 것은 OnItemActivated가 스폰한 액터뿐이다.
	if (Profile->IsInstant())
	{
		// 상태 태그가 없어 슬롯의 태그 경로가 돌지 않는다. 사용 연출은 여기서 직접 낸다.
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->PlayInstantUseFeedback(Profile);
		}

		bItemStarted = true;
		OnItemActivated(*Unit, *Profile);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/HasAuthority(&ActivationInfo), /*bWasCancelled=*/false);
		return;
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
	AppliedEffect = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);

	if (HasAuthority(&ActivationInfo))
	{
		if (AppliedEffect.IsValid())
		{
			UAbilityTask_WaitGameplayEffectRemoved* const Wait = UAbilityTask_WaitGameplayEffectRemoved::WaitForGameplayEffectRemoved(this, AppliedEffect);
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

void UItemAbility::FinishItem()
{
	if (!IsActive())
	{
		return;
	}

	// GE를 먼저 걷는다. 서버에서는 그 제거가 WaitGameplayEffectRemoved를 깨워 EndFromTimer로 오고,
	// 그 뒤의 EndAbility는 IsActive가 거짓이라 아무것도 안 한다. 클라이언트는 태그만 내려간다.
	if (AppliedEffect.IsValid())
	{
		BP_RemoveGameplayEffectFromOwnerWithHandle(AppliedEffect);
		AppliedEffect = FActiveGameplayEffectHandle();
	}
	EndFromTimer();
}

bool UItemAbility::WantsInput(EItemAbilityInput Input) const
{
	return bAiming;
}

void UItemAbility::HandleInput(EItemAbilityInput Input)
{
	if (Input == EItemAbilityInput::Confirm)
	{
		ConfirmAim();
	}
	else
	{
		CancelAim();
	}
}

void UItemAbility::ConfirmAim()
{
	if (!bAiming)
	{
		return;
	}

	EndAim(/*bConfirmed=*/true);
	StartItem();
}

void UItemAbility::CancelAim()
{
	if (!bAiming)
	{
		return;
	}

	EndAim(/*bConfirmed=*/false);

	// 슬롯은 손대지 않았으므로 아이템은 아직 거기 있다. 스펙도 "끝나면 제거" 표시가 서지
	// 않았으므로(ConsumeHeldItem이 하는 일이다) 다시 쓸 수 있다.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/IsAuthority(), /*bWasCancelled=*/true);
}

void UItemAbility::EndAim(bool bConfirmed)
{
	if (!bAiming)
	{
		return;
	}
	bAiming = false;

	AUnit* const Unit = GetUnit();
	const UItemProfile* const Profile = GetItemProfile();
	if (!Unit || !Profile)
	{
		return;
	}

	if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
	{
		Slot->SetAiming(false);
	}

	OnAimEnded(*Unit, *Profile, bConfirmed);
}

void UItemAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 조준 중에 다른 이유로 끝났다(사망, 재발동, 스턴). 미리보기는 어떤 경로로 끝나든 치워져야 한다.
	EndAim(/*bConfirmed=*/false);

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
