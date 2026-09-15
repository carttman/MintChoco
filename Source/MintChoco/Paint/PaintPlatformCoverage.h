#pragma once

#include "CoreMinimal.h"

class UWorld;
class UStaticMeshComponent;
class FPaintCellGrid;

/** Static platform eligibility, opt-in by exact persistent map package (PIE prefixes are ignored). */
namespace PaintPlatformCoverage
{
	MINTCHOCO_API bool MatchesMap(const FString& WorldPackage, const FString& ConfiguredPackage);
	MINTCHOCO_API bool IsEnabled(const UWorld* World);
	MINTCHOCO_API uint8 ResolveDirections(const UStaticMeshComponent& Mesh, int32 MaterialSlot);
	MINTCHOCO_API void Filter(const UStaticMeshComponent& Mesh, FPaintCellGrid& Grid);
	MINTCHOCO_API bool IsWalkableNormal(const UStaticMeshComponent& Mesh, const FVector& Normal);
	/** Height above a sloped plane at which an upright capsule is tangent, plus a small skin. */
	MINTCHOCO_API float CapsuleCenterHeight(float Radius, float HalfHeight, float NormalZ);
}
