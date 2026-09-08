#include "Weapons/PaintDeposit.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"

bool FPaintDeposit::IsPaintable(const FHitResult& Hit)
{
	const AActor* const Actor = Hit.GetActor();
	return Actor && Actor->FindComponentByClass<UPaintableComponent>();
}

FPaintSplat FPaintDeposit::BuildSplat(const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const
{
	check(BrushProfile);
	return BrushProfile->BuildSplat(Hit, IncidentVelocity, PaintId, SplatVolume, HeightAdd, Seed);
}

void FPaintDeposit::MarkTransience(FPaintSplat& Splat, const FHitResult& Hit)
{
	const AActor* const Actor = Hit.GetActor();
	const UPaintableComponent* const Paintable = Actor ? Actor->FindComponentByClass<UPaintableComponent>() : nullptr;
	Splat.bTransient = Paintable && !Paintable->IsWorldNormalPersistent(Hit.ImpactNormal);
}

bool FPaintDeposit::ApplyHit(UWorld* World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const
{
	UPaintSubsystem* const Paint = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
	if (!BrushProfile || !Paint || !IsPaintable(Hit))
	{
		return false;
	}

	FPaintSplat Splat = BuildSplat(Hit, IncidentVelocity, PaintId, Seed);
	MarkTransience(Splat, Hit);
	Paint->SubmitSplat(Splat);
	return true;
}
