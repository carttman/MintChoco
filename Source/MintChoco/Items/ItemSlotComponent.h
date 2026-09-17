#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"

#include "Items/ItemAbility.h"
#include "Items/ItemProfile.h"
#include "Weapons/PaintWeaponProfile.h"

#include "ItemSlotComponent.generated.h"

class UAbilitySystemComponent;
class UAnimSequenceBase;
class UAudioComponent;
class UItemProfile;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPaintGunProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemSlotChangedSignature, UItemProfile*, Item);

/** 오라 하나. 어느 아이템의 것인지(상태 태그)와 그동안 씌울 머티리얼. */
USTRUCT()
struct FItemAuraEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;
};

/**
 * 효과 중인 아이템들의 오라. 오버레이 머티리얼은 메시당 한 칸뿐인데 아이템 효과는 겹칠 수
 * 있으므로(무한 탄환이 도는 중에 스피드 스타를 써도 태그는 둘 다 산다) 켜진 순서를 들고
 * 마지막 것을 보여 준다. EffectItem이 마지막에 켜진 아이템을 가리키는 것과 같은 규칙이다.
 *
 * 월드 없이 테스트한다.
 */
USTRUCT()
struct MINTCHOCO_API FItemAuraStack
{
	GENERATED_BODY()

	/** 오라를 켠다. Material이 없으면(오라를 정하지 않은 아이템) 아무 일도 하지 않는다. */
	void Push(const FGameplayTag& Tag, UMaterialInterface* Material);

	/** 그 태그의 오라를 끈다. 켜진 적 없는 태그면 아무 일도 하지 않는다. */
	void Pop(const FGameplayTag& Tag);

	/** 지금 보여야 할 오라. 켜진 것이 없으면 nullptr. */
	UMaterialInterface* Top() const;

private:
	/** 켜진 순서. 마지막이 보이는 것이다. */
	UPROPERTY()
	TArray<FItemAuraEntry> Entries;
};

/**
 * 유닛의 아이템 슬롯. 한 칸이고, 새 아이템을 밟으면 들고 있던 것을 버리고 교체한다.
 *
 * 습득은 서버가 정한다(GiveItem): 프로필의 어빌리티를 ASC에 주고 HeldItem을 복제한다.
 * 사용은 소유 클라이언트가 시작한다(TryUseHeldItem): 소유자에게 복제된 스펙을 그 자리에서
 * 예측 활성화하고, 서버는 RPC로 따라온다. 어빌리티가 켜지면 서버가 ConsumeHeldItem으로
 * 슬롯을 비운다.
 *
 * 효과의 "지금 무슨 아이템이 걸려 있나"는 GE가 붙이는 상태 태그(State.Item.*)가 진실이고
 * 모든 머신에 복제된다. 이 컴포넌트는 그 태그의 변화를 받아 연출을 켜고 끄며, 스피드
 * 스타 태그는 무브먼트의 속도 부스트 플래그로 옮긴다(대시와 같은 압축 플래그 경로).
 */
UCLASS(ClassGroup = (Items), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UItemSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemSlotComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 들고 있는 아이템. 없으면 nullptr. 모든 머신에 복제된다. */
	UFUNCTION(BlueprintPure, Category = "Item")
	UItemProfile* GetHeldItem() const { return HeldItem; }

	/**
	 * 애니메이션 노티파이(UAnimNotify_ItemSound)가 소리 낼 아이템. 자세를 정하는 아이템(조준 중이면
	 * 슬롯의 것, 효과 중이면 그것)이 먼저고, 없으면 마지막으로 연출을 시작한 아이템(즉발 아이템의
	 * 사용 동작은 효과가 없어 이쪽으로 온다). 아무것도 없으면 nullptr.
	 */
	const UItemProfile* GetAnimationItem() const;

	/**
	 * 애니메이션 구간 노티파이(UAnimNotifyState_ItemSound)가 부른다. 태그의 소리를 캐릭터 메시에
	 * 붙여 틀고 들고 있는다. 같은 태그가 이미 울리고 있으면 그것을 끄고 새로 튼다. 뱅크는
	 * GetAnimationItem의 것이다.
	 */
	void PlayAnimationSound(const FGameplayTag& Tag, FName Socket);

	/** PlayAnimationSound로 튼 소리를 끈다. 없으면 아무 일도 없다. */
	void StopAnimationSound(const FGameplayTag& Tag, float FadeOut);

	/** 서버 전용. 아이템을 슬롯에 넣는다. 들고 있던 것은 버려진다(효과 중인 것은 끝까지 돈다). */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void GiveItem(UItemProfile* Item);

	/** 서버 전용. 어빌리티가 켜진 직후 슬롯을 비우고, 효과가 끝나면 스펙도 걷어 가게 표시한다. */
	void ConsumeHeldItem();

	/** 소유 클라이언트(또는 리슨 호스트)가 사용 키를 눌렀을 때. 활성화가 시작됐으면 true. */
	UFUNCTION(BlueprintCallable, Category = "Item")
	bool TryUseHeldItem();

	/**
	 * 좌클릭. 효과 중인 아이템이 가져갔으면 true이고, 그러면 무기는 쏘지 않는다.
	 * 꿀풍선은 조준을 확정해 던지고, 히어로 랜딩은 정지 중이면 그 자리에서 내리꽂는다.
	 */
	bool HandleFireInput();

	/** 우클릭. 조준 중인 아이템이 있으면 물리고 true. 아이템은 슬롯에 남는다. */
	bool HandleCancelInput();

	/**
	 * 마무리 동작만 남은 아이템(UItemAbility::IsRecovering)을 지금 끝낸다. 발사와 아이템 사용이
	 * 부른다: 그 입력은 마무리가 끝나기를 기다리지 않고 바로 나가야 한다. 마무리 중인 것이 없으면
	 * 아무 일도 없다. Except는 지금 켜지는 어빌리티 자신이다.
	 *
	 * 어빌리티는 서버와 소유 클라이언트에만 있으므로 그 둘에서만 끊긴다. 구경꾼은 서버가 풀어 준
	 * 자세 교체(PoseOverride)의 복제로 따라온다.
	 */
	void InterruptItemRecovery(const UItemAbility* Except = nullptr);

	/**
	 * 지금 유지할 아이템 자세. 없으면 nullptr. 애님 인스턴스가 매 프레임 읽어 ABP로 넘긴다.
	 *
	 * 효과 중인 아이템은 복제되는 상태 태그에서, 조준 중인 아이템은 복제되는 bAiming에서
	 * 나오므로 모든 머신에서 같은 자세가 나온다.
	 */
	UFUNCTION(BlueprintPure, Category = "Item")
	UAnimSequenceBase* GetItemPose() const;

	/** 그 자세가 덮는 범위. 자세가 없으면 전신(기본값)을 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "Item")
	EItemPoseBlend GetItemPoseBlend() const;

	/**
	 * 효과가 도는 중에 어빌리티가 자세를 갈아 끼울 때. 프로필의 PoseAnimation보다 우선하고,
	 * nullptr을 넣으면 다시 프로필 값으로 돌아간다.
	 *
	 * 스위트 스피너가 시작(상체) → 회전(전신) → 끝(상체)을 이걸로 넘긴다. 대시와 같은 규칙이다:
	 * 서버와 소유 클라이언트가 각자 세우고, 나머지에게는 서버가 복제한다.
	 */
	void SetItemPoseOverride(UAnimSequenceBase* Animation, EItemPoseBlend Blend);

	/**
	 * 조준형 아이템이 조준을 시작하거나 끝낼 때 어빌리티가 부른다. 조준 중에는 슬롯을 비우지
	 * 않으므로, 무엇을 조준하는지는 HeldItem이 그대로 알려 준다.
	 *
	 * 대시와 같은 규칙이다: 소유 클라이언트는 예측으로 바로 세우고 서버가 나머지에게 복제한다.
	 */
	void SetAiming(bool bNewAiming);

	bool IsAimingItem() const { return bAiming; }

	/**
	 * 즉발 아이템(Duration 0)의 사용 연출. 이 머신에서 재생하고, 서버라면 구경꾼에게도 보낸다.
	 *
	 * 즉발은 GE를 걸지 않아 상태 태그가 없고, 그래서 태그 변화로 도는 평소의 연출 경로가 아예
	 * 돌지 않는다. 어빌리티가 발동하는 자리에서 직접 불러 준다.
	 */
	void PlayInstantUseFeedback(const UItemProfile* Item);

	/**
	 * 디버그. UItemSettings 목록의 Index번째(0부터) 아이템을 바로 슬롯에 넣는다. 클라이언트는
	 * 서버에 부탁한다. Shipping 빌드에서는 아무 일도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item|Debug")
	void DebugGiveItem(int32 Index);

	/**
	 * 클라이언트가 보낸 속도 부스트 플래그를 서버가 인정해도 되는지. 효과 태그가 이미
	 * 있거나, 그 효과를 켤 아이템을 아직 들고 있을 때(RPC가 무브보다 늦게 오는 창) 참이다.
	 */
	bool IsSpeedBoostAuthorized() const;

	/** 스피너의 산탄을 다른 머신에서 연출로 재생한다. 진짜 공을 날린 서버만 건너뛴다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpinnerShot(const UPaintGunProfile* Volley, const FPaintShot& Shot);

	/**
	 * 한 번 터지고 마는 아이템 연출을 모든 머신에서 재생한다. 서버가 부른다.
	 *
	 * 붙이지 않고 월드 좌표에 두는 것이 요점이다: 사용자가 곧바로 솟아올라도 이펙트는 터진
	 * 자리에 남는다. 어빌리티는 서버와 소유 클라이언트에만 있으므로 구경하는 머신에는 이
	 * 경로로만 닿는다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFXAt(UNiagaraSystem* System, FVector_NetQuantize Location, float Scale);

	/**
	 * 유닛에 붙여 Duration 동안 띄워 두는 아이템 연출. 서버가 부른다.
	 *
	 * 루프하는 이펙트를 쓰므로 스스로 꺼지지 않는다. 각 머신이 Duration 뒤에 Deactivate만 하고,
	 * 남은 파티클이 제 수명을 마치면 bAutoDestroy가 컴포넌트를 치운다. 시작 시각만 보내고
	 * 길이는 각자 재는 구조라 RPC가 한 번이면 된다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttachedFX(UNiagaraSystem* System, float Scale, float ZOffset, float Duration);

	/**
	 * 잉크병 오버라이드(무한 탄환)의 점멸 타이머를 다시 시작한다. 어빌리티가 (재)발동 때 부른다:
	 * 갱신은 태그 수가 1에서 1로 머물러 태그 콜백이 오지 않기 때문이다.
	 */
	void RestartInkLook(float Duration);

	/** 슬롯 내용이 바뀔 때마다, 모든 머신에서. HUD가 여기에 붙는다. */
	UPROPERTY(BlueprintAssignable, Category = "Item")
	FItemSlotChangedSignature OnHeldItemChanged;

protected:
	/** VisibleInstanceOnly인 이유: 에디터 도구(MCP)가 PIE 중에 읽을 수 있으려면 Edit/Visible 지정자가 필요하다. */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_HeldItem, Category = "Item")
	TObjectPtr<UItemProfile> HeldItem;

	UFUNCTION()
	void OnRep_HeldItem();

	/**
	 * 조준형 아이템을 조준하는 중인지.
	 *
	 * 조준은 GE도 상태 태그도 남기지 않으므로(그래서 취소가 되돌릴 것이 없다) 다른 머신에는
	 * 아무 신호도 가지 않는다. 자세를 보여 주려면 이것 하나가 필요하다. 소유자는 예측으로
	 * 이미 알고 있으므로 제외한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bAiming = false;

	/** 조준이 복제로 도착했을 때(소유자가 아닌 머신). 손에 든 물건을 그 값에 맞춘다. */
	UFUNCTION()
	void OnRep_Aiming();

private:
	/**
	 * 입력을 처리할 어빌리티를 로컬에서 찾아 넘기고, 조준의 확정·취소라면 서버에도 알린다.
	 * 히어로 랜딩처럼 무브먼트 플래그로 끝나는 입력은 무브에 실려 가므로 RPC가 없다.
	 */
	bool RouteItemInput(EItemAbilityInput Input);

	/** 이 입력을 가져가겠다는 활성 아이템 어빌리티. 없으면 nullptr. */
	UItemAbility* FindItemAbilityForInput(EItemAbilityInput Input) const;

	/** 조준 중인 아이템이 있으면 물린다. 스턴과 새 아이템 습득이 부른다. */
	void CancelItemAim();

	UFUNCTION(Server, Reliable)
	void ServerItemInput(EItemAbilityInput Input);

	/** 즉발 아이템의 사용 연출을 구경꾼에게. 서버와 소유자는 이미 재생했으므로 건너뛴다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastItemUsed(const UItemProfile* Item);

	/** 사용 동작 + 이펙트 + 소리를 이 머신에서 한 번. 데디케이티드 서버에서는 아무것도 안 한다. */
	void PlayUseFeedback(const UItemProfile& Item);

	/** 사용 동작만. 슬롯에 동적 몽타주로 얹으므로 애님 그래프에 그 이름의 Slot 노드가 있어야 한다. */
	void PlayUseAnimation(const UItemProfile& Item);

	UFUNCTION(Server, Reliable)
	void ServerDebugGiveItem(int32 Index);

	/** 서버 전용. 설정 목록의 Index번째 아이템을 준다. 범위 밖이면 경고. */
	void GiveItemByIndex(int32 Index);

	UAbilitySystemComponent* GetAbilitySystem() const;
	bool HasAuthority() const;
	void SetHeldItem(UItemProfile* Item);

	void HandleTagChanged(const FGameplayTag Tag, int32 NewCount);
	void ApplySpeedBoost(bool bEnabled);
	void StartEffectFeedback(const UItemProfile& Item, const FGameplayTag& Tag);
	void StopEffectFeedback(const FGameplayTag& Tag);

	/**
	 * 스폰한 이펙트에 이 폰의 팀 색을 넣는다(User.TintColor). 다른 이펙트와 같은 규칙이라,
	 * 그 파라미터를 선언하지 않은 시스템은 조용히 무시된다 — 팀 색을 쓰지 않는 아이템 연출도
	 * 그대로 둘 수 있다.
	 *
	 * 반드시 활성화 **전에** 부른다. 이미 켜진 컴포넌트에 넣은 유저 파라미터는 다음 틱으로
	 * 미뤄지는데(SetVariable_Deferred), 첫 틱에 한 번 터지고 마는 연출은 그 한 장이 에셋
	 * 기본색으로 태어난다.
	 */
	void TintTeamFX(UNiagaraComponent* FX) const;

	/** 스택의 맨 위 오라를 캐릭터 메시의 오버레이에 맞춘다. 데디케이티드 서버는 지나간다. */
	void UpdateAura();

	/** 잉크병을 오버라이드 재질로 바꾸고 마지막 1초에 점멸을 예약한다. bOn이 거짓이면 전부 되돌린다. */
	void SetInkLook(bool bOn, float Duration);
	void StartInkBlink();

	/** 서버 전용. HeldItem에 해당하는 스펙. 비우면 효과 종료 시 제거된다. */
	FGameplayAbilitySpecHandle HeldSpec;

	/**
	 * 효과가 도는 아이템. 상태 태그가 오르내릴 때만 갱신된다. 유지 자세와 그 범위를 여기서 읽는다.
	 * 태그는 모든 머신에 복제되므로 이 값도 모든 머신에서 같다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UItemProfile> EffectItem;

	/**
	 * 이 머신에서 마지막으로 연출(사용 동작·발동음)을 시작한 아이템. 즉발 아이템은 효과가 없어
	 * EffectItem에 남지 않으므로, 사용 동작 안의 노티파이가 어느 뱅크를 쓸지 이걸로 안다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<const UItemProfile> LastFeedbackItem;

	/**
	 * 어빌리티가 효과 중에 갈아 끼운 자세. 프로필의 값보다 우선한다. 효과 하나 안에서 구간마다
	 * 자세가 바뀌는 아이템(스위트 스피너)이 쓴다. 소유자는 자기 어빌리티로 이미 알고 있다.
	 */
	UPROPERTY(Replicated)
	TObjectPtr<UAnimSequenceBase> PoseOverride;

	UPROPERTY(Replicated)
	EItemPoseBlend PoseOverrideBlend = EItemPoseBlend::FullBody;

	/** 지금 자세를 정하는 아이템. 조준 중이면 슬롯의 것, 아니면 효과가 도는 것. 없으면 nullptr. */
	const UItemProfile* GetPoseItem() const;

	/** 상태 태그별로 켜 둔 이펙트. 태그가 내려가면 끈다. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UNiagaraComponent>> EffectComponents;

	/**
	 * 상태 태그별로 켜 둔 지속음(Audio.Item.Loop). 이펙트와 같은 수명이고 같은 신호를 따른다:
	 * 태그가 오르면 켜고 내려가면 끈다. 태그별이라 두 버프가 겹치면 두 루프가 같이 돈다.
	 *
	 * 노티파이가 트는 AnimationSounds와는 따로 둔다. 저쪽은 애님 클립의 구간에 묶여 있어
	 * 자세가 바뀌면 같이 끝나지만, 이쪽은 효과가 끝날 때까지 살아 있어야 한다.
	 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UAudioComponent>> EffectSounds;

	/**
	 * 효과가 도는 아이템들의 오라. 이펙트와 같은 이유로 복제하지 않는다: 태그가 모든 머신에
	 * 복제되므로 각자 세우면 같은 그림이 나온다.
	 */
	UPROPERTY(Transient)
	FItemAuraStack AuraStack;

	/**
	 * 애니메이션 구간 노티파이가 틀어 둔 소리. 오디오 태그별 하나. 구간이 끝나면 노티파이가 끄고,
	 * 효과가 끝나거나(조기 종료 포함) 컴포넌트가 사라지면 여기서 전부 끈다.
	 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UAudioComponent>> AnimationSounds;

	/** AnimationSounds를 전부 끈다. */
	void StopAllAnimationSounds(float FadeOut);

	FDelegateHandle TagEventHandle;
	FTimerHandle InkBlinkTimer;
};
