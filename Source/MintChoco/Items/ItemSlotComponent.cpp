#include "Items/ItemSlotComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Game/Unit.h"
#include "Game/UnitMovementComponent.h"
#include "Ink/InkBottleComponent.h"
#include "Items/ItemAbility.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "MintChoco.h"
#include "Weapons/PaintGunProfile.h"

UItemSlotComponent::UItemSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 산탄 멀티캐스트가 이 컴포넌트를 타므로 복제가 필요하다.
	SetIsReplicatedByDefault(true);
}

void UItemSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem())
	{
		TagEventHandle = AbilitySystem->RegisterGenericGameplayTagEvent().AddUObject(this, &UItemSlotComponent::HandleTagChanged);
	}
}

void UItemSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem())
	{
		AbilitySystem->RegisterGenericGameplayTagEvent().Remove(TagEventHandle);
	}
	TagEventHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UItemSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UItemSlotComponent, HeldItem);

	// 소유자는 자기가 조준을 시작했다는 것을 이미 안다. 보내면 지연된 값이 예측을 되돌린다.
	DOREPLIFETIME_CONDITION(UItemSlotComponent, bAiming, COND_SkipOwner);
}

UAbilitySystemComponent* UItemSlotComponent::GetAbilitySystem() const
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
}

bool UItemSlotComponent::HasAuthority() const
{
	// 소유자 없는 컴포넌트는 테스트다. 권한이 있는 것으로 친다.
	const AActor* const Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

void UItemSlotComponent::GiveItem(UItemProfile* Item)
{
	if (!HasAuthority() || !Item)
	{
		return;
	}

	Item->LogUnsetReferences(GetOwner());

	UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem();
	if (AbilitySystem && Item->AbilityClass)
	{
		// 들고만 있던 이전 아이템은 스펙째 버린다. 효과 중인 것은 ConsumeHeldItem이 이미
		// "끝나면 제거"로 표시했으므로 그대로 둔다.
		if (HeldSpec.IsValid())
		{
			if (const FGameplayAbilitySpec* const Previous = AbilitySystem->FindAbilitySpecFromHandle(HeldSpec))
			{
				if (!Previous->IsActive())
				{
					AbilitySystem->ClearAbility(HeldSpec);
				}
			}
			HeldSpec = FGameplayAbilitySpecHandle();
		}

		// 효과가 도는 중에 같은 아이템을 다시 주웠다. 스펙은 이미 있으니 다시 주지 않고,
		// 다시 쓸 수 있도록 "끝나면 제거" 표시만 걷는다. TryActivateAbility는 그 표시가 선
		// 스펙을 거부한다.
		if (FGameplayAbilitySpec* const Existing = AbilitySystem->FindAbilitySpecFromClass(Item->AbilityClass))
		{
			Existing->RemoveAfterActivation = false;
			Existing->SourceObject = Item;
			HeldSpec = Existing->Handle;
		}
		else
		{
			HeldSpec = AbilitySystem->GiveAbility(FGameplayAbilitySpec(Item->AbilityClass, 1, INDEX_NONE, Item));
		}
	}

	SetHeldItem(Item);
}

void UItemSlotComponent::ConsumeHeldItem()
{
	if (!HasAuthority())
	{
		return;
	}

	if (HeldSpec.IsValid())
	{
		if (UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem())
		{
			AbilitySystem->SetRemoveAbilityOnEnd(HeldSpec);
		}
		HeldSpec = FGameplayAbilitySpecHandle();
	}

	SetHeldItem(nullptr);
}

void UItemSlotComponent::SetHeldItem(UItemProfile* Item)
{
	if (HeldItem == Item)
	{
		return;
	}

	HeldItem = Item;

	// RepNotify는 값을 바꾼 권한 쪽에서는 불리지 않으므로 서버(리슨 호스트 포함)는 직접 알린다.
	OnHeldItemChanged.Broadcast(HeldItem);

	// 조준 중에 다른 아이템을 주웠다. 확정되면 엉뚱한 아이템이 소모되므로 조준을 물린다.
	// 양쪽이 같은 복제 값을 보고 각자 물리므로 RPC가 필요 없다(아래 OnRep도 같은 이유다).
	if (HeldItem)
	{
		CancelItemAim();
	}
}

void UItemSlotComponent::OnRep_HeldItem()
{
	OnHeldItemChanged.Broadcast(HeldItem);

	if (HeldItem)
	{
		CancelItemAim();
	}
}

bool UItemSlotComponent::TryUseHeldItem()
{
	UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem();
	if (!HeldItem || !HeldItem->AbilityClass || !AbilitySystem)
	{
		return false;
	}

	// 소유자에게는 스펙이 복제되어 있으므로 그 자리에서 예측 활성화한다. HeldItem이
	// 스펙보다 먼저 도착한 찰나에 누르면 실패하지만, 다시 누르면 된다.
	return AbilitySystem->TryActivateAbilityByClass(HeldItem->AbilityClass);
}

bool UItemSlotComponent::HandleFireInput()
{
	return RouteItemInput(EItemAbilityInput::Confirm);
}

bool UItemSlotComponent::HandleCancelInput()
{
	return RouteItemInput(EItemAbilityInput::Cancel);
}

bool UItemSlotComponent::RouteItemInput(EItemAbilityInput Input)
{
	UItemAbility* const Ability = FindItemAbilityForInput(Input);
	if (!Ability)
	{
		return false;
	}

	// 조준의 확정·취소는 서버도 알아야 한다: 던지는 것도, 슬롯을 비우거나 그대로 두는 것도 서버다.
	// 먼저 로컬에서 처리하고 알린다. 순서가 반대면 아래 호출이 이미 끝난 어빌리티를 만난다.
	const bool bNeedsServer = Ability->IsAiming() && !HasAuthority();
	Ability->HandleInput(Input);

	if (bNeedsServer)
	{
		ServerItemInput(Input);
	}
	return true;
}

void UItemSlotComponent::ServerItemInput_Implementation(EItemAbilityInput Input)
{
	UItemAbility* const Ability = FindItemAbilityForInput(Input);

	// 조준 중인 것만 받는다. 서버의 인스턴스가 아직 조준에 들어가지 않았다면(활성화 RPC와
	// 경합) 아무 일도 하지 않는다: 아이템은 슬롯에 남으므로 다시 쓰면 된다.
	if (Ability && Ability->IsAiming())
	{
		Ability->HandleInput(Input);
	}
}

UItemAbility* UItemSlotComponent::FindItemAbilityForInput(EItemAbilityInput Input) const
{
	UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem();
	if (!AbilitySystem)
	{
		return nullptr;
	}

	FScopedAbilityListLock ListLock(*AbilitySystem);
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		for (UGameplayAbility* const Instance : Spec.GetAbilityInstances())
		{
			UItemAbility* const Item = Cast<UItemAbility>(Instance);
			if (Item && Item->IsActive() && Item->WantsInput(Input))
			{
				return Item;
			}
		}
	}
	return nullptr;
}

const UItemProfile* UItemSlotComponent::GetPoseItem() const
{
	// 조준이 먼저다: 조준 중에는 아직 효과가 시작되지 않았고, 그 아이템은 아직 슬롯에 있다.
	return (bAiming && HeldItem) ? ToRawPtr(HeldItem) : ToRawPtr(EffectItem);
}

UAnimSequenceBase* UItemSlotComponent::GetItemPose() const
{
	const UItemProfile* const Item = GetPoseItem();
	return Item ? Item->PoseAnimation : nullptr;
}

EItemPoseBlend UItemSlotComponent::GetItemPoseBlend() const
{
	const UItemProfile* const Item = GetPoseItem();
	return Item ? Item->PoseBlend : EItemPoseBlend::FullBody;
}

void UItemSlotComponent::SetAiming(bool bNewAiming)
{
	bAiming = bNewAiming;
}

void UItemSlotComponent::PlayInstantUseFeedback(const UItemProfile* Item)
{
	if (!Item)
	{
		return;
	}

	// 서버(리슨 호스트 포함)와 예측 발동한 소유 클라이언트가 여기를 지난다. 둘 다 지금 재생하고,
	// 서버만 나머지에게 보낸다.
	PlayUseFeedback(*Item);

	if (HasAuthority())
	{
		MulticastItemUsed(Item);
	}
}

void UItemSlotComponent::MulticastItemUsed_Implementation(const UItemProfile* Item)
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	if (!Item || HasAuthority() || (Pawn && Pawn->IsLocallyControlled()))
	{
		// 서버는 보내는 자리에서, 소유자는 예측 발동 때 이미 재생했다. 남은 구경꾼만 여기까지 온다.
		return;
	}
	PlayUseFeedback(*Item);
}

void UItemSlotComponent::PlayUseFeedback(const UItemProfile& Item)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayUseAnimation(Item);

	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USceneComponent* const AttachTo = Character && Character->GetMesh() ? Character->GetMesh() : GetOwner()->GetRootComponent();

	if (Item.ActivateSound)
	{
		UGameplayStatics::SpawnSoundAttached(Item.ActivateSound, AttachTo);
	}

	// 즉발 아이템에는 끌 시점이 없으므로 스스로 정리되게 둔다(지속형은 태그가 내려갈 때 끈다).
	if (Item.ActivateFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			Item.ActivateFX, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true);
	}
}

void UItemSlotComponent::PlayUseAnimation(const UItemProfile& Item)
{
	UAnimSequenceBase* const Animation = Item.UseAnimation.Animation;
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Animation || !AnimInstance)
	{
		return;
	}

	// 스켈레톤이 다른 클립은 재생 자체는 되지만 포즈가 적용되지 않아, 아무 일도 일어나지 않은
	// 것처럼 보인다. 프로필의 칸에는 스켈레톤 필터가 없어서 다른 캐릭터의 클립을 고르기 쉽다.
	const USkeletalMesh* const MeshAsset = Mesh->GetSkeletalMeshAsset();
	const USkeleton* const MeshSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	if (MeshSkeleton && Animation->GetSkeleton() != MeshSkeleton)
	{
		UE_LOG(LogMintChoco, Warning,
			TEXT("%s: %s의 사용 동작 %s는 스켈레톤이 다릅니다(%s ≠ %s). 재생해도 보이지 않습니다."),
			*GetNameSafe(GetOwner()), *Item.GetName(), *Animation->GetName(),
			*GetNameSafe(Animation->GetSkeleton()), *GetNameSafe(MeshSkeleton));
		return;
	}

	// 몽타주 에셋 없이 시퀀스를 슬롯에 얹는다(AUnit::PlayFeedbackMontage와 같은 경로다).
	const UAnimMontage* const Played = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Animation, Item.UseAnimation.Slot, Item.UseAnimation.BlendIn, Item.UseAnimation.BlendOut);

	if (!Played)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: %s의 사용 동작 %s를 재생하지 못했습니다."),
			*GetNameSafe(GetOwner()), *Item.GetName(), *Animation->GetName());
		return;
	}

	// 여기까지 왔는데 화면에 아무것도 없다면 애님 그래프에 그 이름의 Slot 노드가 없는 것이다.
	// 엔진은 그 경우를 실패로 치지 않는다: 몽타주는 정상적으로 돌고 소비하는 노드만 없다.
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: %s의 사용 동작 %s를 %s 슬롯에 올렸습니다."),
		*GetNameSafe(GetOwner()), *Item.GetName(), *Animation->GetName(), *Item.UseAnimation.Slot.ToString());
}

void UItemSlotComponent::CancelItemAim()
{
	UItemAbility* const Ability = FindItemAbilityForInput(EItemAbilityInput::Cancel);
	if (Ability && Ability->IsAiming())
	{
		Ability->HandleInput(EItemAbilityInput::Cancel);
	}
}

bool UItemSlotComponent::IsSpeedBoostAuthorized() const
{
	if (HeldItem && HeldItem->GrantsSpeedBoost())
	{
		return true;
	}

	const UAbilitySystemComponent* const AbilitySystem = GetAbilitySystem();
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_SpeedStar);
}

void UItemSlotComponent::HandleTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 스턴에 걸리면 조준이 끊긴다(스턴 중에는 아이템을 쓸 수 없으므로 조준만 걸려 있는 것도
	// 이상하다). 아이템은 슬롯에 남는다. 태그는 모든 머신에 복제되므로 각자 물린다.
	if (NewCount > 0 && Tag == ItemTags::State_Status_Stunned)
	{
		CancelItemAim();
	}

	// 부모 태그(State.Item, State)의 변화도 같이 오므로 아이템에 해당하는 잎 태그만 받는다.
	// 프로필을 그대로 붙잡아 두므로(EffectItem) const로 받지 않는다.
	UItemProfile* const Item = UItemSettings::Get().FindItemByStateTag(Tag);
	if (!Item)
	{
		return;
	}

	const bool bActive = NewCount > 0;
	if (Item->GrantsSpeedBoost())
	{
		ApplySpeedBoost(bActive);
	}
	if (Item->OverridesInkLook())
	{
		SetInkLook(bActive, Item->Duration);
	}

	// 유지 자세는 태그를 따라간다. 태그는 모든 머신에 복제되므로 자세도 어디서나 같다.
	if (bActive)
	{
		EffectItem = Item;
		StartEffectFeedback(*Item, Tag);
	}
	else
	{
		if (EffectItem == Item)
		{
			EffectItem = nullptr;
		}
		StopEffectFeedback(Tag);
	}
}

void UItemSlotComponent::ApplySpeedBoost(bool bEnabled)
{
	// 플래그는 이 캐릭터의 움직임을 실제로 계산하는 쪽만 세운다. 시뮬레이션 프록시의 속도는
	// 복제된 위치에서 나오므로 만질 이유가 없다.
	AUnit* const Unit = Cast<AUnit>(GetOwner());
	if (!Unit || (!Unit->IsLocallyControlled() && !Unit->HasAuthority()))
	{
		return;
	}

	if (UUnitMovementComponent* const Movement = Unit->GetUnitMovement())
	{
		Movement->SetWantsSpeedBoost(bEnabled);
	}
}

void UItemSlotComponent::StartEffectFeedback(const UItemProfile& Item, const FGameplayTag& Tag)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayUseAnimation(Item);

	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USceneComponent* const AttachTo = Character && Character->GetMesh() ? Character->GetMesh() : GetOwner()->GetRootComponent();

	if (Item.ActivateSound)
	{
		UGameplayStatics::SpawnSoundAttached(Item.ActivateSound, AttachTo);
	}

	// 갱신(같은 아이템 재사용)은 태그 수가 1에서 1로 머물러 여기까지 오지 않는다. 그래도
	// 이미 켜진 것이 있으면 겹치지 않게 그대로 둔다.
	if (Item.ActivateFX && !EffectComponents.Contains(Tag))
	{
		UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Item.ActivateFX, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true);
		if (FX)
		{
			EffectComponents.Add(Tag, FX);
		}
	}
}

void UItemSlotComponent::StopEffectFeedback(const FGameplayTag& Tag)
{
	TObjectPtr<UNiagaraComponent> FX;
	if (EffectComponents.RemoveAndCopyValue(Tag, FX) && FX)
	{
		FX->Deactivate();
	}
}

void UItemSlotComponent::MulticastSpinnerShot_Implementation(const UPaintGunProfile* Volley, const FPaintShot& Shot)
{
	// 서버는 진짜 공을 날렸고, 소유자는 회전만 예측하므로 소유자에게는 이것이 첫 공이다.
	// 소유자를 건너뛰지 않는다: 무기와 달리 소유자가 자기 산탄을 예측하지 않기 때문이다.
	if (HasAuthority() || !Volley || !GetWorld())
	{
		return;
	}
	Volley->PlayCosmetic(*GetWorld(), Cast<APawn>(GetOwner()), Shot);
}

void UItemSlotComponent::RestartInkLook(float Duration)
{
	SetInkLook(true, Duration);
}

void UItemSlotComponent::SetInkLook(bool bOn, float Duration)
{
	const AUnit* const Unit = Cast<AUnit>(GetOwner());
	UInkBottleComponent* const Bottle = Unit ? Unit->GetInkBottle() : nullptr;
	UWorld* const World = GetWorld();
	if (!Bottle || !World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(InkBlinkTimer);
	Bottle->SetBlink(false);
	Bottle->SetLookOverride(bOn);
	if (!bOn)
	{
		return;
	}

	// 마지막 1초는 점멸. 지속시간이 1초 이하면 처음부터 점멸한다.
	const float BlinkAt = Duration - 1.0f;
	if (BlinkAt > 0.0f)
	{
		World->GetTimerManager().SetTimer(InkBlinkTimer, this, &UItemSlotComponent::StartInkBlink, BlinkAt, /*bLoop=*/false);
	}
	else
	{
		StartInkBlink();
	}
}

void UItemSlotComponent::StartInkBlink()
{
	const AUnit* const Unit = Cast<AUnit>(GetOwner());
	if (UInkBottleComponent* const Bottle = Unit ? Unit->GetInkBottle() : nullptr)
	{
		Bottle->SetBlink(true, 0.1f);
	}
}

void UItemSlotComponent::DebugGiveItem(int32 Index)
{
#if !UE_BUILD_SHIPPING
	if (HasAuthority())
	{
		GiveItemByIndex(Index);
	}
	else
	{
		ServerDebugGiveItem(Index);
	}
#endif
}

void UItemSlotComponent::ServerDebugGiveItem_Implementation(int32 Index)
{
#if !UE_BUILD_SHIPPING
	GiveItemByIndex(Index);
#endif
}

void UItemSlotComponent::GiveItemByIndex(int32 Index)
{
	TArray<UItemProfile*> Items;
	UItemSettings::Get().LoadItems(Items);
	if (!Items.IsValidIndex(Index))
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 아이템 %d번이 없다(목록 %d개)."), *GetNameSafe(GetOwner()), Index + 1, Items.Num());
		return;
	}
	GiveItem(Items[Index]);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 디버그로 %s를 받았다."), *GetNameSafe(GetOwner()), *GetNameSafe(Items[Index]));
}
