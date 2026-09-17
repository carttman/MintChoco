#include "Weapons/PaintDeposit.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Game/Unit.h"
#include "MintChoco.h"
#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintSplashProfile.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"
#include "Weapons/PaintHitReceiver.h"

bool FPaintDeposit::IsPaintable(const FHitResult& Hit)
{
	const AActor* const Actor = Hit.GetActor();
	return Actor && Actor->FindComponentByClass<UPaintableComponent>();
}

bool FPaintDeposit::ReceivesSplat(const FHitResult& Hit)
{
	if (IsPaintable(Hit))
	{
		return true;
	}
	const UStaticMeshComponent* const Mesh = Cast<UStaticMeshComponent>(Hit.GetComponent());
	return Mesh && Mesh->Mobility != EComponentMobility::Movable;
}

FPaintSplat FPaintDeposit::BuildSplat(const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const
{
	check(BrushProfile);
	return BrushProfile->BuildSplat(Hit, IncidentVelocity, PaintId, SplatVolume, GetHeightAdd(), Seed);
}

bool FPaintDeposit::KeepsPaint(const FHitResult& Hit)
{
	const AActor* const Actor = Hit.GetActor();
	const UPaintableComponent* const Paintable = Actor ? Actor->FindComponentByClass<UPaintableComponent>() : nullptr;
	return Paintable && Paintable->IsWorldNormalPersistent(Hit.ImpactNormal);
}

void FPaintDeposit::MarkTransience(FPaintSplat& Splat, const FHitResult& Hit)
{
	Splat.bTransient = !KeepsPaint(Hit);
}

bool FPaintDeposit::StrikeReceiver(const FHitResult& Hit, uint8 PaintId) const
{
	AActor* const Actor = Hit.GetActor();
	if (!Actor || !Actor->GetClass()->ImplementsInterface(UPaintHitReceiver::StaticClass()))
	{
		return false;
	}
	IPaintHitReceiver::Execute_ReceivePaintHit(Actor, HitPower, PaintId, Hit);
	return true;
}

bool FPaintDeposit::StrikeUnit(const FHitResult& Hit, uint8 PaintId, float Charge) const
{
	AUnit* const Unit = Cast<AUnit>(Hit.GetActor());
	if (!Unit)
	{
		return false;
	}

	// 아군 판정은 이 id 비교 하나뿐이다. 두 값을 나란히 찍어 두면, 팀이 아직 정해지지 않은
	// 유닛이나 무기가 들고 있던 예약 id가 섞여 들어올 때 로그에서 바로 드러난다.
	const int32 VictimPaintId = Unit->GetPaintId();
	const int32 ShotPaintId = PaintId;
	if (VictimPaintId == ShotPaintId)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("[스턴][적중] %s 통과: 같은 페인트 id %d."),
			*GetNameSafe(Unit), VictimPaintId);
		return false;
	}

	const float StunSeconds = StunSecondsFor(Charge);
	const bool bStunned = Unit->TryApplyStun(StunSeconds, StunSuperArmorDuration);
	UE_LOG(LogMintChoco, Verbose, TEXT("[스턴][적중] %s(페인트 id %d) ← 탄 id %d, 충전 %.2f, 스턴 %.2f초 → %s."),
		*GetNameSafe(Unit), VictimPaintId, ShotPaintId, Charge, StunSeconds, bStunned ? TEXT("적용") : TEXT("거절"));
	return bStunned;
}

bool FPaintDeposit::ApplyHit(UWorld* World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed, float Charge, uint8 StarGen, float BallRadius) const
{
	// A receiver is struck before the surface test: a balloon is not a paintable surface, yet the
	// hit that bursts it is a hit all the same. A unit is not a surface either; it takes the stun.
	StrikeReceiver(Hit, PaintId);
	StrikeUnit(Hit, PaintId, Charge);

	UPaintSubsystem* const Paint = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
	if (!BrushProfile || !Paint || !ReceivesSplat(Hit))
	{
		return false;
	}

	FPaintSplat Splat = BuildSplat(Hit, IncidentVelocity, PaintId, Seed);
	Splat.StarGen = StarGen;
	if (Splash)
	{
		// The splash is expanded wherever the splat is applied, so the contact travels with it.
		const double Speed = IncidentVelocity.Size();
		Splat.Splash = Splash;
		Splat.IncidentDir = Speed > UE_DOUBLE_KINDA_SMALL_NUMBER ? IncidentVelocity / Speed : -FVector(Hit.ImpactNormal);
		Splat.IncidentSpeed = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt32(Speed), 0, static_cast<int32>(MAX_uint16)));
		Splat.BallRadius = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(BallRadius), 0, static_cast<int32>(MAX_uint8)));
	}
	MarkTransience(Splat, Hit);
	Paint->SubmitSplat(Splat);
	return true;
}
