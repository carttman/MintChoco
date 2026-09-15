#include "Items/HeroLandingAbility.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include "Game/Unit.h"
#include "Game/UnitMovementComponent.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/HeroLandingProfile.h"
#include "Items/ItemAreaEffect.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/LandingMarker.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"

UGA_HeroLanding::UGA_HeroLanding()
{
	StateTag = ItemTags::State_Item_HeroLanding;
	EffectClass = UGE_HeroLanding::StaticClass();
}

bool UGA_HeroLanding::WantsInput(EItemAbilityInput Input) const
{
	if (Input != EItemAbilityInput::Confirm)
	{
		return false;
	}

	// 이미 내리꽂는 중이면 받을 것이 없다. 어차피 이 상태에서는 상태 태그가 방아쇠를 막고
	// 있으므로, 여기서 가져가지 않아도 총은 나가지 않는다.
	const AUnit* const Unit = GetUnit();
	const UUnitMovementComponent* const Movement = Unit ? Unit->GetUnitMovement() : nullptr;
	if (!Movement)
	{
		return false;
	}

	const EHeroLandingPhase Phase = Movement->GetHeroLandingPhase();
	return Phase == EHeroLandingPhase::Rise || Phase == EHeroLandingPhase::Hover;
}

void UGA_HeroLanding::HandleInput(EItemAbilityInput Input)
{
	if (Input != EItemAbilityInput::Confirm)
	{
		return;
	}

	// 의도만 세운다. 실제로 언제 꽂히는지는 단계 기계가 정하고(상승 중에 눌렀다면 정점에
	// 닿는 순간), 서버는 무브에 실려 온 같은 플래그로 같은 판단을 한다.
	AUnit* const Unit = GetUnit();
	UUnitMovementComponent* const Movement = Unit ? Unit->GetUnitMovement() : nullptr;
	if (Movement && Unit->IsLocallyControlled())
	{
		Movement->SetWantsHeroDive(true);
	}
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

	// 착지점 표시는 소유 클라이언트(리슨 호스트 포함)만.
	UWorld* const World = Unit.GetWorld();
	if (Unit.IsLocallyControlled() && World && World->GetNetMode() != NM_DedicatedServer && Landing->AimMarkerClass)
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

		UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
		Tick->OnTick.AddDynamic(this, &UGA_HeroLanding::HandleTick);
		Tick->ReadyForActivation();
	}
}

void UGA_HeroLanding::HandleTick(float DeltaTime)
{
	const AUnit* const Unit = GetUnit();
	const UUnitMovementComponent* const Movement = Unit ? Unit->GetUnitMovement() : nullptr;
	if (!Marker || !Movement)
	{
		return;
	}

	// 착지점이 정해진 뒤에는 고정.
	const EHeroLandingPhase Phase = Movement->GetHeroLandingPhase();
	if (Phase == EHeroLandingPhase::Dive || Phase == EHeroLandingPhase::Approach)
	{
		Marker->SetActorHiddenInGame(false);
		Marker->SetActorLocation(Movement->GetHeroDiveTarget());
		return;
	}

	// 그 전에는 지금 조준하는 곳. 내려설 수 없는 곳(벽, 급경사, 사거리 밖, 허공)을 보고 있으면
	// 표시를 감춘다 — 그 상태에서는 좌클릭도 듣지 않으므로, 표시가 곧 "지금 꽂을 수 있다"는 뜻이다.
	FVector Target;
	const bool bCanLand = Movement->ComputeAimTarget(Target);
	Marker->SetActorHiddenInGame(!bCanLand);

	// 버틴 만큼 안쪽 원이 자란다. 내리꽂기에 들어서면 그때 굳은 값이 그대로 남아,
	// 떨어지는 동안 보이는 원이 실제로 터질 크기다.
	if (ALandingMarker* const Rings = Cast<ALandingMarker>(Marker))
	{
		Rings->SetCharge(Movement->GetHeroCharge());
	}
	if (bCanLand)
	{
		Marker->SetActorLocation(Target);
	}
}

void UGA_HeroLanding::HandleLanded()
{
	AUnit* const Unit = GetUnit();
	UWorld* const World = Unit ? Unit->GetWorld() : nullptr;
	if (Unit && World && Landing && IsAuthority())
	{
		const float HalfHeight = Unit->GetCapsuleComponent() ? Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		const FVector Origin = Unit->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight - 20.0f);

		// 공중에서 버틴 만큼 세다. 무브먼트가 시간으로만 정하는 값이라 서버가 스스로 안다.
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
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 히어로 랜딩 착지."), *GetNameSafe(Unit));
	}

	FinishItem();
}

void UGA_HeroLanding::OnItemEnded(AUnit& Unit, const UItemProfile& Profile)
{
	Unit.OnHeroLandingFinished.Remove(LandedHandle);
	LandedHandle.Reset();
	DestroyMarker();

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
