#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ItemAbility.generated.h"

class AUnit;
class UItemGameplayEffect;
class UItemProfile;
struct FGameplayEffectRemovalInfo;

/**
 * 효과 중인 아이템이 무기에서 가져갈 수 있는 입력. 좌클릭이 확정, 우클릭이 취소다.
 *
 * AUnit의 발사 입력이 UItemSlotComponent에게 먼저 묻고, 가져간 입력은 무기에 닿지 않는다.
 */
UENUM()
enum class EItemAbilityInput : uint8
{
	/** 좌클릭. 꿀풍선은 던지고, 히어로 랜딩은 내리꽂는다. */
	Confirm,
	/** 우클릭. 조준을 물리고 아이템을 슬롯에 되돌린다. */
	Cancel,
};

/**
 * 아이템 효과의 공통 골격. 서브클래스는 OnItemActivated / OnItemEnded만 채운다.
 *
 * 습득 시 서버가 스펙(SourceObject = 프로필)을 주고, 사용 키를 누른 소유 클라이언트가
 * LocalPredicted로 활성화한다. 활성화되면 권한 쪽이 슬롯을 비우고, 양쪽 모두 지속형
 * GE(EffectClass)에 프로필의 Duration을 SetByCaller로 넣어 건다. 상태 태그(StateTag)는
 * 스펙의 DynamicGrantedTags로 붙어 모든 머신에 복제되며, 슬롯 컴포넌트가 그 태그를
 * 보고 연출과 속도 플래그를 움직인다.
 *
 * 끝나는 시점은 서버와 클라이언트가 다르게 잰다. 서버는 자기 GE가 제거될 때, 클라이언트는
 * Duration이 지났을 때다. 클라이언트가 예측 적용한 GE 핸들에 제거 대기를 걸면 예측 키가
 * 확정되는 순간(RTT 뒤) 예측 GE가 치워지면서 콜백이 와서 효과가 일찍 끝나 버린다.
 *
 * 같은 아이템을 효과 중에 다시 쓰면 bRetriggerInstancedAbility가 이 인스턴스를 끝내고
 * 다시 활성화하며, GE는 스택 갱신으로 타이머만 다시 시작한다.
 *
 * 즉발 아이템(프로필 Duration 0)은 GE 없이 OnItemActivated 직후 끝난다. 효과는 그 안에서
 * 스폰한 액터(투사체, 돔, 살포)가 이어받는다. 지속형이지만 일찍 끝날 수 있는 아이템(히어로
 * 랜딩)은 FinishItem으로 GE를 걷고 끝낸다.
 *
 * 조준형 아이템(IsAimingItem, 꿀풍선)은 발동해도 곧바로 효과를 내지 않는다. 슬롯도 비우지
 * 않고 GE도 걸지 않은 채 확정(좌클릭)이나 취소(우클릭)를 기다리며, 확정된 뒤에야 위의 흐름이
 * 시작된다. 그래서 취소하면 아이템이 슬롯에 그대로 남는다 — 되돌려 주는 코드가 따로 없는 것은
 * 애초에 가져가지 않았기 때문이다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UItemAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UItemAbility();

	/** 효과 중 ASC에 붙는 태그. 서브클래스가 생성자에서 정한다. */
	FGameplayTag GetStateTag() const { return StateTag; }

	TSubclassOf<UItemGameplayEffect> GetEffectClass() const { return EffectClass; }

	/**
	 * 이 입력을 아이템이 가져가는가. 슬롯 컴포넌트가 무기보다 먼저 묻는다.
	 *
	 * 기본은 조준 중일 때만 참이다. 히어로 랜딩처럼 효과가 도는 중에 입력을 쓰는 아이템은
	 * 따로 덮어쓴다. 참을 돌려주면 그 클릭은 무기에 닿지 않으므로, 쓸 수 있을 때만 참이어야 한다.
	 */
	virtual bool WantsInput(EItemAbilityInput Input) const;

	/** WantsInput이 참일 때만 온다. 입력이 들어온 머신에서 먼저, 서버에서 한 번 더. */
	virtual void HandleInput(EItemAbilityInput Input);

	/** 조준형 아이템이 확정이나 취소를 기다리는 중인가. */
	bool IsAiming() const { return bAiming; }

protected:
	/**
	 * 조준형 아이템인지. 참이면 발동이 효과를 내지 않고 조준 상태로 들어간다. 슬롯은 그대로
	 * 두므로, 취소는 아무것도 되돌리지 않아도 된다.
	 */
	virtual bool IsAimingItem() const { return false; }

	/** 조준이 시작됐다. 서버와 소유 클라이언트 양쪽에서 불린다. 미리보기는 소유 쪽만 켠다. */
	virtual void OnAimStarted(AUnit& Unit, const UItemProfile& Profile) {}

	/** 조준이 끝났다. 확정이든 취소든, 다른 이유로 어빌리티가 끝나는 경우에도 한 번 온다. */
	virtual void OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed) {}

	/** 조준을 확정한다. 여기서부터 평소의 아이템 발동(슬롯 비우기, GE, OnItemActivated)이 돈다. */
	void ConfirmAim();

	/** 조준을 물린다. 슬롯은 건드린 적이 없으므로 아이템이 그대로 남는다. */
	void CancelAim();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** GE가 걸린 직후. 예측 클라이언트와 서버 양쪽에서 불린다. 서버 전용 일은 IsAuthority로 가른다. */
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) {}

	/** 효과가 끝날 때(만료, 리트리거, 취소 모두). OnItemActivated가 불린 인스턴스에서만 온다. */
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) {}

	/**
	 * 지속시간 전에 효과를 끝낸다. 서버는 자기 GE를 걷고 끝을 복제하며, 클라이언트는 자기만 끝낸다.
	 * (서버가 따로 자기 시계로 끝내고 ClientEndAbility가 오므로 두 번 끝나도 안전하다.)
	 */
	void FinishItem();

	AUnit* GetUnit() const;
	const UItemProfile* GetItemProfile() const;
	bool IsAuthority() const;

	/** 사용자 팀의 페인트 id. 팀이 없으면 무기의 id. */
	uint8 GetPaintId() const;

	FGameplayTag StateTag;
	TSubclassOf<UItemGameplayEffect> EffectClass;

private:
	UFUNCTION()
	void HandleEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);

	UFUNCTION()
	void HandleDurationElapsed();

	void EndFromTimer();

	/** 슬롯을 비우고, GE를 걸고, OnItemActivated를 부른다. 조준형이면 확정된 뒤에 온다. */
	void StartItem();

	/** bAiming을 내리고 OnAimEnded를 부른다. 두 번 불려도 안전하다. */
	void EndAim(bool bConfirmed);

	/** 이 인스턴스가 건 GE. 즉발이면 무효. */
	FActiveGameplayEffectHandle AppliedEffect;

	/** OnItemActivated가 불렸는지. EndAbility는 여러 경로로 두 번 올 수 있어 짝을 맞춘다. */
	bool bItemStarted = false;

	/** 조준 중인지. 확정·취소, 또는 어빌리티 종료로 내려간다. */
	bool bAiming = false;
};
