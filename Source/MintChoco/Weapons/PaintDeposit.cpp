#include "Weapons/PaintDeposit.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Paint/PaintBrushProfile.h"
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

bool FPaintDeposit::ApplyHit(UWorld* World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const
{
	// A receiver is struck before the surface test: a balloon is not a paintable surface, yet the
	// hit that bursts it is a hit all the same.
	StrikeReceiver(Hit, PaintId);

	UPaintSubsystem* const Paint = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
	if (!BrushProfile || !Paint || !ReceivesSplat(Hit))
	{
		return false;
	}

	FPaintSplat Splat = BuildSplat(Hit, IncidentVelocity, PaintId, Seed);
	MarkTransience(Splat, Hit);
	Paint->SubmitSplat(Splat);
	return true;
}
