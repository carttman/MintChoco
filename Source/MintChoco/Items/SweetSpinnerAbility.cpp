#include "Items/SweetSpinnerAbility.h"

#include "Abilities/Tasks/AbilityTask_Repeat.h"

#include "Game/Unit.h"
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
	if (UPaintWeaponComponent* const Secondary = Unit.GetSecondaryWeapon())
	{
		Secondary->ReleaseTrigger();
	}

	if (IsAuthority() && Spinner->Volley)
	{
		StartYaw = static_cast<float>(Unit.GetActorRotation().Yaw);
		VolleyCount = Spinner->GetVolleyCount();
		UAbilityTask_Repeat* const Repeat = UAbilityTask_Repeat::RepeatAction(this, Spinner->VolleyInterval, VolleyCount);
		Repeat->OnPerformAction.AddDynamic(this, &UGA_SweetSpinner::HandleVolley);
		Repeat->ReadyForActivation();
	}
}

void UGA_SweetSpinner::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	Spinner = nullptr;
}

void UGA_SweetSpinner::HandleVolley(int32 ActionNumber)
{
	AUnit* const Unit = GetUnit();
	UPaintWeaponComponent* const Weapon = Unit ? Unit->GetPaintWeapon() : nullptr;
	if (!Unit || !Weapon || !Spinner || !Spinner->Volley || !GetWorld())
	{
		return;
	}

	// 이 발의 방향. 캐릭터는 그대로이므로 액터 정면이 아니라 계산한 요를 쓴다.
	// 피치는 프로필이 정한 고정값이라 모든 발이 같은 높이로 나간다.
	const float Yaw = SweetSpinner::VolleyYawDegrees(StartYaw, ActionNumber, VolleyCount, Spinner->Turns);
	const FRotator Rotation(Spinner->PitchDeg, Yaw, 0.0f);

	// 원점은 캐릭터 중심의 손 높이. 손의 총구를 쓰면 한쪽에 고정된 채 몸을 가로질러 쏘게 된다.
	FVector Origin = Unit->GetActorLocation();
	Origin.Z = Weapon->GetMuzzleTransform().GetLocation().Z;

	// 총 프로필의 조준 트레이스는 시선(ViewOrigin, ViewDirection)에서 그 방향의 첫 표면으로 수렴한다.
	FPaintFireContext Context;
	Context.World = GetWorld();
	Context.Instigator = Unit;
	Context.Muzzle = FTransform(Rotation, Origin);
	Context.ViewOrigin = Origin;
	Context.ViewDirection = Rotation.Vector();
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
