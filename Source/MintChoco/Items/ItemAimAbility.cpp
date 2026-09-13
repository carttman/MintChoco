#include "Items/ItemAimAbility.h"

#include "GameFramework/Controller.h"

#include "Game/Unit.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"

void UItemAimAbility::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	bConfirmed = false;

	// 좌클릭은 무기가 아니라 슬롯을 거쳐 온다. 슬롯이 예측 클라이언트와 서버 양쪽에서 알린다.
	if (UItemSlotComponent* const Found = Unit.FindComponentByClass<UItemSlotComponent>())
	{
		Slot = Found;
		ConfirmHandle = Found->OnAimConfirmed.AddUObject(this, &UItemAimAbility::HandleConfirm);
	}

	OnAimBegan(Unit, Profile);

	// 미리보기는 조준하는 사람의 화면에만 있으면 된다. 복제할 것이 없으므로 로컬에서만 만든다.
	if (Unit.IsLocallyControlled())
	{
		Preview = SpawnPreview(Unit, Profile);

		UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
		Tick->OnTick.AddDynamic(this, &UItemAimAbility::HandleTick);
		Tick->ReadyForActivation();
	}
}

void UItemAimAbility::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	if (UItemSlotComponent* const Found = Slot.Get())
	{
		Found->OnAimConfirmed.Remove(ConfirmHandle);
	}
	Slot.Reset();
	ConfirmHandle.Reset();

	DestroyPreview();

	// 쏘지 못한 채 끝났다면(제한 시간 만료, 사망) 서브클래스에게 알린다. 아이템은 활성화
	// 시점에 이미 슬롯에서 빠졌으므로 돌려주지 않는다.
	if (!bConfirmed)
	{
		OnAimAborted(Unit, Profile);
	}
}

void UItemAimAbility::HandleTick(float DeltaTime)
{
	AUnit* const Unit = GetUnit();
	if (!Unit || !Preview)
	{
		return;
	}
	UpdatePreview(*Unit, *Preview, DeltaTime);
}

void UItemAimAbility::HandleConfirm()
{
	if (bConfirmed)
	{
		return;
	}

	AUnit* const Unit = GetUnit();
	const UItemProfile* const Profile = GetItemProfile();
	if (!Unit || !Profile)
	{
		return;
	}

	bConfirmed = true;

	// 미리보기는 효과보다 먼저 치운다. 확정 뒤 한 프레임이라도 남아 있으면 눈에 띈다.
	DestroyPreview();

	OnAimConfirmed(*Unit, *Profile);

	// 조준은 끝났다. 지속시간이 남아 있어도 여기서 효과를 걷는다.
	FinishItem();
}

void UItemAimAbility::DestroyPreview()
{
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}
