#pragma once

#include "CoreMinimal.h"

/**
 * Where a shot's physics starts, as opposed to where it appears to start. The muzzle socket rides
 * the animation: a ball that left it would bend with every breath and stride, leave below the
 * crosshair, and differ between the owner's pose and the server's. The physics therefore leaves
 * the sight line itself, and the mesh slides from the gun onto that path.
 */
namespace PaintAim
{
	/**
	 * The point on the view ray at the pawn's depth, pushed forward by ForwardMargin. It lies on
	 * the sight line by construction, so a ball fired from it flies where the crosshair points until
	 * gravity takes it, and it never moves with the pose. A pawn behind the camera sits at the
	 * margin alone.
	 */
	inline FVector FireOrigin(const FVector& ViewOrigin, const FVector& ViewDirection, const FVector& PawnLocation, float ForwardMargin)
	{
		const double Depth = FMath::Max(FVector::DotProduct(PawnLocation - ViewOrigin, ViewDirection), 0.0) + ForwardMargin;
		return ViewOrigin + ViewDirection * Depth;
	}

	/**
	 * How far a ball's mesh has slid from the gun's muzzle onto the real flight path, 0 to 1, eased
	 * so the merge neither pops at the start nor stops dead. A MergeSeconds of 0 merges at once.
	 */
	inline float MergeAlpha(float Elapsed, float MergeSeconds)
	{
		if (MergeSeconds <= 0.0f || Elapsed >= MergeSeconds) return 1.0;

		return FMath::SmoothStep(0.0f, 1.0f, FMath::Max(Elapsed, 0.0f) / MergeSeconds);
	}
}
