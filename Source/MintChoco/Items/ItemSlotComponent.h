#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"

#include "Weapons/PaintWeaponProfile.h"

#include "ItemSlotComponent.generated.h"

class UAbilitySystemComponent;
class UItemProfile;
class UNiagaraComponent;
class UNiagaraSystem;
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

private:
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

	/** 상태 태그별로 켜 둔 이펙트. 태그가 내려가면 끈다. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UNiagaraComponent>> EffectComponents;

	FDelegateHandle TagEventHandle;
	FTimerHandle InkBlinkTimer;
};
