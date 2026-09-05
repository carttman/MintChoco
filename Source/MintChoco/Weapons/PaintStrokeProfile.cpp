#include "Weapons/PaintStrokeProfile.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Paint/PaintLog.h"

UPaintStrokeProfile::UPaintStrokeProfile()
{
	FireMode = EPaintFireMode::Continuous;
}

void UPaintStrokeProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!Deposit.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no BrushProfile, its strokes will not paint."),
		*GetNameSafe(Owner), *GetName());
}

bool UPaintStrokeProfile::Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke) const
{
	if (!Context.World)
	{
		return false;
	}

	// Any surface within Reach of the muzzle lies within Reach + |camera - muzzle| of the camera,
	// so the view ray only has to look that far.
	const FVector MuzzleLocation = Context.Muzzle.GetLocation();
	const float TraceLength = Reach + FVector::Dist(Context.ViewOrigin, MuzzleLocation);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintStroke), /*bTraceComplex=*/false, Context.Instigator);

	FHitResult Hit;
	const bool bHit = Context.World->LineTraceSingleByChannel(Hit,
		Context.ViewOrigin,
		Context.ViewOrigin + Context.ViewDirection * TraceLength,
		ECC_Visibility, Params);
	if (!bHit || !FPaintDeposit::IsPaintable(Hit) || FVector::Dist(Hit.ImpactPoint, MuzzleLocation) > Reach)
	{
		// Dropping the anchor means sweeping off a surface and back on starts a fresh stroke
		// rather than one that jumps the gap.
		Stroke.Reset();
		return false;
	}

	if (Stroke.bAnchorValid && FVector::DistSquared(Hit.ImpactPoint, Stroke.Anchor) < FMath::Square(StrokeSpacing))
	{
		return false;
	}

	if (!Deposit.ApplyHit(Context.World, Hit, Context.ViewDirection * NominalImpactSpeed, Context.PaintId, Context.Seed))
	{
		return false;
	}
	
	Stroke.Anchor = Hit.ImpactPoint;
	Stroke.bAnchorValid = true;
	return true;
}
