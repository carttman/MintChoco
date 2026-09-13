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
class UItemProfile;
class UNiagaraComponent;
class UPaintGunProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemSlotChangedSignature, UItemProfile*, Item);

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
	UPROPERTY(Replicated)
	bool bAiming = false;

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

	/** 지금 자세를 정하는 아이템. 조준 중이면 슬롯의 것, 아니면 효과가 도는 것. 없으면 nullptr. */
	const UItemProfile* GetPoseItem() const;

	/** 상태 태그별로 켜 둔 이펙트. 태그가 내려가면 끈다. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UNiagaraComponent>> EffectComponents;

	FDelegateHandle TagEventHandle;
	FTimerHandle InkBlinkTimer;
};
