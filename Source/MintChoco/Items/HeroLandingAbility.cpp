#include "Items/HeroLandingAbility.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

#include "Game/Unit.h"
#include "Game/UnitMovementComponent.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/HeroLandingProfile.h"
#include "Items/ItemAreaEffect.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSlotComponent.h"
#include "Items/LandingMarker.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"

namespace
{
	/** 발밑. 착지 때 탄을 뿌리는 지점과 같은 높이라 시작·착지 이펙트가 바닥에 붙는다. */
	FVector GroundOrigin(const AUnit& Unit)
	{
		const float HalfHeight = Unit.GetCapsuleComponent() ? Unit.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		return Unit.GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight - 20.0f);
	}
}

UGA_HeroLanding::UGA_HeroLanding()
{
	StateTag = ItemTags::State_Item_HeroLanding;
	EffectClass = UGE_HeroLanding::StaticClass();
}

void UGA_HeroLanding::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	Landing = Cast<UHeroLandingProfile>(&Profile);
	UUnitMovementComponent* const Movement = Unit.GetUnitMovement();
	if (!Landing || !Movement)
	{
		return;
	}

	Movement->SetHeroLandingParams(Landing->Landing);

	// 의도는 움직임을 실제로 계산하는 쪽이 세운다. 원격 클라이언트의 폰이라면 서버는 무브에 실려
	// 오는 플래그로 따라간다: 서버가 먼저 세우면 아직 플래그가 없는 옛 무브가 도착해 시작을 되돌린다.
	if (Unit.IsLocallyControlled())
	{
		Movement->SetWantsHeroLanding(true);
	}

	LandedHandle = Unit.OnHeroLandingFinished.AddUObject(this, &UGA_HeroLanding::HandleLanded);

	// 출발 연출. 어빌리티는 서버와 소유 클라이언트에만 있으므로, 구경하는 머신까지 닿도록
	// 서버만 부르고 멀티캐스트가 뿌린다.
	if (IsAuthority() && Landing->StartFX)
	{
		if (UItemSlotComponent* const Slot = Unit.GetItemSlot())
		{
			Slot->MulticastPlayFXAt(Landing->StartFX, GroundOrigin(Unit), Landing->StartFXScale);
		}
	}

	// 착지점 표시와 카메라는 소유 클라이언트(리슨 호스트 포함)만. 마커가 없어도 카메라를
	// 되돌릴 틱은 돌아야 하므로 AimMarkerClass는 마커 스폰에서만 따진다.
	UWorld* const World = Unit.GetWorld();
	const bool bLocalCosmetic = Unit.IsLocallyControlled() && World && World->GetNetMode() != NM_DedicatedServer;
	if (bLocalCosmetic)
	{
		if (Landing->AimMarkerClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = &Unit;
			Marker = World->SpawnActor<AActor>(Landing->AimMarkerClass, Unit.GetActorLocation(), FRotator::ZeroRotator, Params);
			if (ALandingMarker* const Rings = Cast<ALandingMarker>(Marker))
			{
				// 바깥 원은 끝까지 버텼을 때의 스턴 반경이다. 충전 배율이 여기에 곱해지므로
				// 최대값을 그대로 넘기면 된다.
				Rings->SetMaxRadius(Landing->StunRadius);
			}
			if (Marker)
			{
				// 표시일 뿐이라 아무것도 막지 않는다. 마커는 이 머신에만 있으므로, 충돌을 켜 두면
				// 내 화면에서만 다른 플레이어가 밀리거나 걸려 서버와 어긋난다.
				Marker->SetActorEnableCollision(false);
			}
		}

		SetCameraRaised(Unit, true);
	}

	// 서버도 틱이 필요하다: 호버 진입을 보고 이펙트를 뿌리는 쪽이 서버다. 단계 기계는 서버에서도
	// 같은 값으로 돌기 때문에 여기서 읽어도 된다.
	if (bLocalCosmetic || IsAuthority())
	{
		UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
		Tick->OnTick.AddDynamic(this, &UGA_HeroLanding::HandleTick);
		Tick->ReadyForActivation();
	}
}

void UGA_HeroLanding::SetCameraRaised(AUnit& Unit, bool bRaise)
{
	USpringArmComponent* const Boom = Unit.GetCameraBoom();
	if (!Boom || !Landing || Landing->CameraRiseOffset <= 0.0f || bCameraRaised == bRaise)
	{
		return;
	}

	if (bRaise)
	{
		// 월드 기준 오프셋이라 어디를 보고 있든 위로 간다. SocketOffset은 붐과 함께 돌아
		// 시선에 따라 방향이 바뀐다.
		SavedBoomOffset = Boom->TargetOffset;
		Boom->TargetOffset = SavedBoomOffset + FVector(0.0f, 0.0f, Landing->CameraRiseOffset);
	}
	else
	{
		Boom->TargetOffset = SavedBoomOffset;
	}
	bCameraRaised = bRaise;
}

void UGA_HeroLanding::HandleTick(float DeltaTime)
{
	AUnit* const Unit = GetUnit();
	const UUnitMovementComponent* const Movement = Unit ? Unit->GetUnitMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const EHeroLandingPhase Phase = Movement->GetHeroLandingPhase();

	// 호버에 들어서면 발밑 이펙트를 한 번 뿌린다. 호버는 정확히 HoverTime만큼이므로 끄는 시각을
	// 각 머신이 스스로 셀 수 있고, 그래서 RPC가 한 번으로 끝난다.
	if (IsAuthority() && !bHoverFXSent && Phase == EHeroLandingPhase::Hover && Landing && Landing->HoverFX)
	{
		if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
		{
			Slot->MulticastPlayAttachedFX(
				Landing->HoverFX, Landing->HoverFXScale, Landing->HoverFXZOffset,
				Landing->Landing.HoverTime + Landing->HoverFXStopDelay);
		}
		bHoverFXSent = true;
	}

	// "착지하기 시작하는" 순간이 내리꽂기의 시작이다. 정지 시간이 끝나는 그 프레임에 카메라가
	// 원래 높이로 돌아간다. 초를 세지 않으므로 Hover 시간을 바꿔도 알아서 맞는다.
	if (bCameraRaised && Phase == EHeroLandingPhase::Dive)
	{
		SetCameraRaised(*Unit, false);
	}

	if (Marker)
	{
		// 내리꽂기부터는 고정, 그 전에는 지금 조준하는 곳.
		const FVector Target = Phase == EHeroLandingPhase::Dive ? Movement->GetHeroDiveTarget() : Movement->ComputeAimTarget();
		Marker->SetActorLocation(Target);

		// 버틴 만큼 안쪽 원이 자란다. 내리꽂기에 들어서면 그때 굳은 값이 그대로 남아,
		// 떨어지는 동안 보이는 원이 실제로 터질 크기다.
		if (ALandingMarker* const Rings = Cast<ALandingMarker>(Marker))
		{
			Rings->SetCharge(Movement->GetHeroCharge());
		}
	}
}

void UGA_HeroLanding::HandleLanded()
{
	AUnit* const Unit = GetUnit();
	UWorld* const World = Unit ? Unit->GetWorld() : nullptr;
	if (Unit && World && Landing && IsAuthority())
	{
		const FVector Origin = GroundOrigin(*Unit);

		if (Landing->LandFX)
		{
			if (UItemSlotComponent* const Slot = Unit->GetItemSlot())
			{
				Slot->MulticastPlayFXAt(Landing->LandFX, Origin, Landing->LandFXScale);
			}
		}

		// 공중에서 버틴 만큼 세다. 무브먼트가 시간으로만 정하는 값이라 서버가 스스로 안다 —
		// 클라이언트가 보내 주는 것이 아니라서 조작할 여지가 없다.
		const UUnitMovementComponent* const Movement = Unit->GetUnitMovement();
		const float Scale = Landing->ChargeScaleFor(Movement ? Movement->GetHeroCharge() : 1.0f);

		FPaintBurstParams Burst = Landing->Burst;
		Burst.PaintId = GetPaintId();
		Burst.Seed = FMath::Rand();
		// 파열 반경은 v² ÷ (980 × 중력배율)이라 속도의 제곱에 비례한다. 반경을 Scale배로
		// 하려면 속도는 √Scale배다.
		Burst.Speed *= FMath::Sqrt(Scale);
		// 탄 수까지 줄여야 약한 착지가 실제로 덜 칠한다. 최소 한 발은 남긴다.
		Burst.Count = FMath::Max(FMath::RoundToInt32(Burst.Count * Scale), 1);
		APaintBurst::Spawn(*World, Origin, Burst);

		FItemAreaEffect::Apply(*World, Origin, Landing->StunRadius * Scale, Unit->GetTeam(), Unit, /*bKnockback=*/true);
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 히어로 랜딩 착지(효과 배율 %.2f)."), *GetNameSafe(Unit), Scale);
	}

	FinishItem();
}

void UGA_HeroLanding::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	Unit.OnHeroLandingFinished.Remove(LandedHandle);
	LandedHandle.Reset();
	DestroyMarker();

	// 착지로 끝나면 틱이 이미 되돌렸다. 상한이나 취소로 끝난 경우를 위해 한 번 더 확인한다.
	SetCameraRaised(Unit, false);

	// 착지 전에 끝났다(상한, 취소). 상승·정지 중이면 낙하로 돌아가고, 내리꽂기는 착지까지 간다.
	if (UUnitMovementComponent* const Movement = Unit.GetUnitMovement())
	{
		const EHeroLandingPhase Phase = Movement->GetHeroLandingPhase();
		if (Phase == EHeroLandingPhase::Rise || Phase == EHeroLandingPhase::Hover)
		{
			Movement->AbortHeroLanding();
		}
		else if (Phase == EHeroLandingPhase::None && Unit.IsLocallyControlled())
		{
			Movement->SetWantsHeroLanding(false);
		}

		// 조기 낙하 의도가 남으면 다음 발동의 호버가 첫 프레임에 끝난다. 무브먼트가 착지와
		// 중단에서 이미 내리지만, 어느 길로 끝나든 확실히 0이 되게 한 번 더 내린다.
		Movement->SetWantsHeroDive(false);
	}
}

void UGA_HeroLanding::DestroyMarker()
{
	if (Marker)
	{
		Marker->Destroy();
		Marker = nullptr;
	}
}
