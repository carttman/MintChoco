#include "Items/SweetSpinnerAbility.h"

#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "Game/Unit.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"
#include "Items/SweetSpinnerProfile.h"
#include "MintChoco.h"
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

	if (!IsAuthority() || !Spinner->Volley)
	{
		return;
	}

	StartYaw = static_cast<float>(Unit.GetActorRotation().Yaw);

	// 회전 구간에서만 쏜다. 애니메이션에 표시가 없으면 지속시간 전체가 구간이다.
	float SpinStart = 0.0f;
	float SpinEnd = 0.0f;
	Spinner->GetVolleyWindow(SpinStart, SpinEnd);
	VolleyCount = Spinner->GetVolleyCount(SpinEnd - SpinStart);

	// 준비 동작이 있으면 그동안은 쏘지 않는다. 애니메이션은 어빌리티가 켜지는 순간부터 도므로
	// 시퀀스의 시각이 곧 여기서의 대기 시간이다.
	if (SpinStart > UE_KINDA_SMALL_NUMBER)
	{
		UAbilityTask_WaitDelay* const Delay = UAbilityTask_WaitDelay::WaitDelay(this, SpinStart);
		Delay->OnFinish.AddDynamic(this, &UGA_SweetSpinner::StartVolleys);
		Delay->ReadyForActivation();
		return;
	}

	StartVolleys();
}

void UGA_SweetSpinner::StartVolleys()
{
	if (!Spinner || !Spinner->Volley)
	{
		return;
	}

	UAbilityTask_Repeat* const Repeat = UAbilityTask_Repeat::RepeatAction(this, Spinner->VolleyInterval, VolleyCount);
	Repeat->OnPerformAction.AddDynamic(this, &UGA_SweetSpinner::HandleVolley);
	Repeat->ReadyForActivation();
}

void UGA_SweetSpinner::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	Spinner = nullptr;
}

bool UGA_SweetSpinner::ComputeHandMuzzle(const AUnit& Unit, const USweetSpinnerProfile& Profile, FVector& OutOrigin, FVector& OutFlatDirection)
{
	const USkeletalMeshComponent* const Mesh = Unit.GetMesh();
	if (!Mesh || Profile.HandSocket.IsNone() || !Mesh->DoesSocketExist(Profile.HandSocket))
	{
		return false;
	}

	OutOrigin = Mesh->GetSocketLocation(Profile.HandSocket);

	// 몸 중심에서 손으로 뻗은 수평 방향. 손이 도는 대로 방향도 돈다.
	FVector Flat = OutOrigin - Unit.GetActorLocation();
	Flat.Z = 0.0f;
	if (!Flat.Normalize())
	{
		// 손이 몸 중심 바로 위아래다(만세 자세 등). 방향을 뽑을 수 없다.
		return false;
	}

	OutFlatDirection = Flat;
	return true;
}

void UGA_SweetSpinner::HandleVolley(int32 ActionNumber)
{
	AUnit* const Unit = GetUnit();
	UPaintWeaponComponent* const Weapon = Unit ? Unit->GetPaintWeapon() : nullptr;
	if (!Unit || !Weapon || !Spinner || !Spinner->Volley || !GetWorld())
	{
		return;
	}

	// 원점과 방향 모두 손에서 온다: 애니메이션이 도는 대로 탄이 나가므로 따로 맞출 것이 없다.
	FVector Origin;
	FVector Flat;
	if (!ComputeHandMuzzle(*Unit, *Spinner, Origin, Flat))
	{
		// 손 소켓을 못 쓴다. 예전 방식으로 돈다: 캐릭터 중심의 손 높이에서 계산한 요로.
		//
		// 캐릭터마다 조용히 달라지면 원인을 찾기 어려우므로 첫 발에서 한 번 알린다. 소켓이
		// 있는데도 여기로 왔다면 손이 몸 중심 바로 위아래라 수평 방향을 뽑지 못한 것이다.
		const USkeletalMeshComponent* const Mesh = Unit->GetMesh();
		UE_CLOG(ActionNumber == 0, LogMintChoco, Warning,
			TEXT("%s: 손 소켓 '%s'로 쏘지 못해 캐릭터 중심에서 쏩니다(메시 %s, 소켓·본 있음: %s)."),
			*GetNameSafe(Unit), *Spinner->HandSocket.ToString(),
			*GetNameSafe(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr),
			(Mesh && Mesh->DoesSocketExist(Spinner->HandSocket)) ? TEXT("예") : TEXT("아니오"));

		Flat = FRotator(0.0f, SweetSpinner::VolleyYawDegrees(StartYaw, ActionNumber, VolleyCount, Spinner->Turns), 0.0f).Vector();
		Origin = Unit->GetActorLocation();
		Origin.Z = Weapon->GetMuzzleTransform().GetLocation().Z;
	}

	// 피치는 프로필이 정한 고정값이라 모든 발이 같은 높이로 나간다.
	const FRotator Rotation(Spinner->PitchDeg, static_cast<float>(Flat.Rotation().Yaw), 0.0f);

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
