#include "Items/SweetSpinnerAbility.h"

#include "Abilities/Tasks/AbilityTask_Repeat.h"

#include "Game/Unit.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"
#include "Items/SweetSpinnerProfile.h"
#include "Weapons/PaintGunProfile.h"
#include "Weapons/PaintWeaponComponent.h"

UGA_SweetSpinner::UGA_SweetSpinner()
{
	StateTag = ItemTags::State_Item_SweetSpinner;
	EffectClass = UGE_SweetSpinner::StaticClass();
}

void UGA_SweetSpinner::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	Spinner = Cast<USweetSpinnerProfile>(&Profile);
	if (!Spinner)
	{
		return;
	}

	// 무기의 방아쇠는 상태 태그가 막지만, 이미 당겨진 채였다면 여기서 놓아야 한다.
	if (UPaintWeaponComponent* const Weapon = Unit.GetPaintWeapon())
	{
		Weapon->ReleaseTrigger();
	}

	Unit.bUseControllerRotationYaw = false;

	UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
	Tick->OnTick.AddDynamic(this, &UGA_SweetSpinner::HandleTick);
	Tick->ReadyForActivation();

	if (IsAuthority() && Spinner->Volley)
	{
		const int32 VolleyCount = FMath::Max(1, FMath::RoundToInt(Profile.Duration / Spinner->VolleyInterval));
		UAbilityTask_Repeat* const Repeat = UAbilityTask_Repeat::RepeatAction(this, Spinner->VolleyInterval, VolleyCount);
		Repeat->OnPerformAction.AddDynamic(this, &UGA_SweetSpinner::HandleVolley);
		Repeat->ReadyForActivation();
	}
}

void UGA_SweetSpinner::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	// 다시 켜는 순간 컨트롤 요로 스냅한다. 회전은 이미 여러 바퀴 돈 뒤라 어디서 멈추든 같다.
	Unit.bUseControllerRotationYaw = true;
	Spinner = nullptr;
}

void UGA_SweetSpinner::HandleTick(float DeltaTime)
{
	AUnit* const Unit = GetUnit();
	if (Unit && Spinner)
	{
		Unit->AddActorWorldRotation(FRotator(0.0f, Spinner->SpinRateDeg * DeltaTime, 0.0f));
	}
}

void UGA_SweetSpinner::HandleVolley(int32 ActionNumber)
{
	AUnit* const Unit = GetUnit();
	UPaintWeaponComponent* const Weapon = Unit ? Unit->GetPaintWeapon() : nullptr;
	if (!Unit || !Weapon || !Spinner || !Spinner->Volley || !GetWorld())
	{
		return;
	}

	// 조준점 대신 액터 정면. 총구에서 정면으로 보는 시선을 넘기면 총 프로필의 조준 트레이스가
	// 그 방향의 첫 표면으로 수렴한다.
	const FTransform Muzzle = Weapon->GetMuzzleTransform();
	FPaintFireContext Context;
	Context.World = GetWorld();
	Context.Instigator = Unit;
	Context.Muzzle = Muzzle;
	Context.ViewOrigin = Muzzle.GetLocation();
	Context.ViewDirection = Unit->GetActorForwardVector();
	Context.PaintId = Weapon->GetPaintId();
	Context.Seed = FMath::Rand();
	Context.bAuthority = true;

	FPaintStrokeState Stroke;
	FPaintShot Shot;
	if (Spinner->Volley->Fire(Context, Stroke, Shot))
	{
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->MulticastSpinnerShot(Spinner->Volley, Shot);
		}
	}
}
