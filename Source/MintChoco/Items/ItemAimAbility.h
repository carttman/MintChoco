#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "ItemAimAbility.generated.h"

class AUnit;
class UItemProfile;
class UItemSlotComponent;

/**
 * 조준하고 좌클릭으로 쏘는 아이템의 공통 골격.
 *
 * 꿀풍선, 디저트 폭격, 히어로 랜딩이 모두 "아이템 키 → 조준 표시 → 좌클릭 확정"이라는 같은
 * 흐름을 쓴다. 서브클래스는 미리보기를 어떻게 그리는지(SpawnPreview / UpdatePreview)와
 * 확정하면 무엇을 하는지(OnAimConfirmed)만 채운다.
 *
 * 조준 중에는 상태 태그(State.Item.Aiming)가 붙어 무기 발사가 막힌다. 그 클릭은 무기 대신
 * 이 어빌리티로 오는데, 경로는 AUnit::StartFire -> UItemSlotComponent::ConfirmAim이다.
 * 슬롯이 예측 클라이언트에서 먼저 알리고 서버에도 보내므로, 양쪽이 같은 순간에 확정한다.
 *
 * 즉발이 아니라 지속형이어야 한다: 프로필의 Duration이 조준 제한 시간이고, 그 안에 쏘지
 * 않으면 효과가 만료되며 아이템은 이미 슬롯에서 빠졌으므로 그대로 잃는다. 취소 입력은 없다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UItemAimAbility : public UItemAbility
{
	GENERATED_BODY()

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override final;
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override final;

	/**
	 * 조준을 시작한다. 미리보기 스폰은 SpawnPreview가 맡으므로, 여기서는 그 밖의 준비만 한다.
	 * 서버와 예측 클라이언트 양쪽에서 불린다.
	 */
	virtual void OnAimBegan(AUnit& Unit, const UItemProfile& Profile) {}

	/**
	 * 조준 미리보기 액터. 보는 사람의 화면에만 있으면 되므로 로컬 조종 머신에서만 불린다.
	 * null을 돌려주면 미리보기 없이 조준만 한다.
	 */
	virtual AActor* SpawnPreview(AUnit& Unit, const UItemProfile& Profile) { return nullptr; }

	/** 매 프레임 조준점을 따라 미리보기를 옮긴다. 미리보기가 있는 머신에서만 불린다. */
	virtual void UpdatePreview(AUnit& Unit, AActor& InPreview, float DeltaTime) {}

	/**
	 * 좌클릭이 들어왔다. 실제 효과는 여기서 낸다. 서버와 예측 클라이언트 양쪽에서 불리므로
	 * 서버 전용 일은 IsAuthority로 가른다. 이 호출 뒤 어빌리티는 스스로 끝난다.
	 */
	virtual void OnAimConfirmed(AUnit& Unit, const UItemProfile& Profile) {}

	/** 조준이 확정되지 않은 채 끝났다(제한 시간 만료, 취소, 사망). 정리할 것이 있으면 여기서. */
	virtual void OnAimAborted(AUnit& Unit, const UItemProfile& Profile) {}

	/** 스폰해 둔 미리보기. 없을 수 있다. */
	AActor* GetPreview() const { return Preview; }

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	void HandleConfirm();

	void DestroyPreview();

	UPROPERTY(Transient)
	TObjectPtr<AActor> Preview;

	TWeakObjectPtr<UItemSlotComponent> Slot;
	FDelegateHandle ConfirmHandle;

	/** 확정은 한 번뿐이다. 슬롯이 예측과 서버 양쪽에서 알려 와도 두 번 쏘지 않는다. */
	bool bConfirmed = false;
};
