#include "Items/ItemSlotComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
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
}

void UItemSlotComponent::OnRep_HeldItem()
{
	OnHeldItemChanged.Broadcast(HeldItem);
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
	// 부모 태그(State.Item, State)의 변화도 같이 오므로 아이템에 해당하는 잎 태그만 받는다.
	const UItemProfile* const Item = UItemSettings::Get().FindItemByStateTag(Tag);
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

	if (bActive)
	{
		StartEffectFeedback(*Item, Tag);
	}
	else
	{
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
			FVector(Item.ActivateFXScale), EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true,
			ENCPoolMethod::None);
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

void UItemSlotComponent::MulticastPlayFXAt_Implementation(UNiagaraSystem* System, FVector_NetQuantize Location, float Scale)
{
	// 리슨 호스트도 그려야 하므로 HasAuthority로 거르지 않는다. 아무도 예측하지 않는 연출이라
	// 어느 머신에서도 두 번 나올 일이 없다.
	UWorld* const World = GetWorld();
	if (!System || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, Location, FRotator::ZeroRotator, FVector(Scale));
}

void UItemSlotComponent::MulticastPlayAttachedFX_Implementation(UNiagaraSystem* System, float Scale, float ZOffset, float Duration)
{
	UWorld* const World = GetWorld();
	AActor* const Owner = GetOwner();
	USceneComponent* const AttachTo = Owner ? Owner->GetRootComponent() : nullptr;
	if (!System || !World || !AttachTo || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 캡슐 루트에 붙인다: 메시는 애니메이션으로 흔들리지만 캡슐은 폰의 위치 그 자체다.
	UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, AttachTo, NAME_None, FVector(0.0f, 0.0f, ZOffset), FRotator::ZeroRotator,
		FVector(Scale), EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true, ENCPoolMethod::None);
	if (!FX || Duration <= 0.0f)
	{
		return;
	}

	// 핸들을 멤버로 들고 있지 않는 이유: 이 연출은 겹쳐 쓸 수 있고, 각자 제 타이머로 꺼지면
	// 슬롯이 상태를 기억할 필요가 없다. 약참조라 폰이 먼저 죽어도 안전하다.
	FTimerHandle StopHandle;
	const TWeakObjectPtr<UNiagaraComponent> WeakFX(FX);
	World->GetTimerManager().SetTimer(
		StopHandle,
		[WeakFX]()
		{
			if (UNiagaraComponent* const Live = WeakFX.Get())
			{
				// 스폰만 멈춘다. 떠 있던 파티클은 제 수명을 마치고 사라진다.
				Live->Deactivate();
			}
		},
		Duration, false);
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
