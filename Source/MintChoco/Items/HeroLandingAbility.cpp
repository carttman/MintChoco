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
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"

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

	// 착지점 표시는 소유 클라이언트(리슨 호스트 포함)만.
	UWorld* const World = Unit.GetWorld();
	if (Unit.IsLocallyControlled() && World && World->GetNetMode() != NM_DedicatedServer && Landing->AimMarkerClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = &Unit;
		Marker = World->SpawnActor<AActor>(Landing->AimMarkerClass, Unit.GetActorLocation(), FRotator::ZeroRotator, Params);

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

	// 내리꽂기부터는 고정, 그 전에는 지금 조준하는 곳.
	const FVector Target = Movement->GetHeroLandingPhase() == EHeroLandingPhase::Dive ? Movement->GetHeroDiveTarget() : Movement->ComputeAimTarget();
	Marker->SetActorLocation(Target);
}

void UGA_HeroLanding::HandleLanded()
{
	AUnit* const Unit = GetUnit();
	UWorld* const World = Unit ? Unit->GetWorld() : nullptr;
	if (Unit && World && Landing && IsAuthority())
	{
		const float HalfHeight = Unit->GetCapsuleComponent() ? Unit->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		const FVector Origin = Unit->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight - 20.0f);

		FPaintBurstParams Burst = Landing->Burst;
		Burst.PaintId = GetPaintId();
		Burst.Seed = FMath::Rand();
		APaintBurst::Spawn(*World, Origin, Burst);

		FItemAreaEffect::Apply(*World, Origin, Landing->StunRadius, Unit->GetTeam(), Unit, /*bKnockback=*/true);
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
