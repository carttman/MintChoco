#include "Paint/PaintPlatformCoverage.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/Unit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintMeshTriangles.h"
#include "Paint/PaintSettings.h"

namespace
{
	const ACharacter* PlatformCharacter(const UWorld* World)
	{
		if (World && World->GetWorldSettings() && World->GetWorldSettings()->DefaultGameMode)
		{
			const AGameModeBase* Mode = World->GetWorldSettings()->DefaultGameMode->GetDefaultObject<AGameModeBase>();
			if (Mode && Mode->DefaultPawnClass)
			{
				if (const ACharacter* Character = Cast<ACharacter>(Mode->DefaultPawnClass->GetDefaultObject()))
				{
					return Character;
				}
			}
		}
		return GetDefault<AUnit>();
	}

	float WalkableZ(const UStaticMeshComponent& Mesh)
	{
		const float DefaultZ = PlatformCharacter(Mesh.GetWorld())->GetCharacterMovement()->GetWalkableFloorZ();
		return Mesh.GetWalkableSlopeOverride().ModifyWalkableFloorZ(DefaultZ);
	}
}

bool PaintPlatformCoverage::MatchesMap(const FString& WorldPackage, const FString& ConfiguredPackage)
{
	return !ConfiguredPackage.IsEmpty() && UWorld::RemovePIEPrefix(WorldPackage) == ConfiguredPackage;
}

bool PaintPlatformCoverage::IsEnabled(const UWorld* World)
{
	return World && MatchesMap(World->GetOutermost()->GetName(), UPaintSettings::Get().PlatformCoverageMap);
}

bool PaintPlatformCoverage::IsWalkableNormal(const UStaticMeshComponent& Mesh, const FVector& Normal)
{
	return Normal.Z >= WalkableZ(Mesh) && Normal.Z > UE_KINDA_SMALL_NUMBER;
}

float PaintPlatformCoverage::CapsuleCenterHeight(float Radius, float HalfHeight, float NormalZ)
{
	return HalfHeight - Radius + Radius / FMath::Max(NormalZ, 0.01f) + 2.0f;
}

uint8 PaintPlatformCoverage::ResolveDirections(const UStaticMeshComponent& Mesh, int32 MaterialSlot)
{
	FPaintMeshTriangles Triangles;
	if (!GatherPaintMeshTriangles(Mesh, MaterialSlot, Triangles))
	{
		return 0;
	}
	uint8 Mask = 0;
	const FTransform Transform = Mesh.GetComponentTransform();
	const FVector Scale = Transform.GetScale3D().GetAbs();
	const float MinZ = WalkableZ(Mesh);
	for (int32 Index = 0; Index + 2 < Triangles.Indices.Num(); Index += 3)
	{
		const uint32 A = Triangles.Indices[Index], B = Triangles.Indices[Index + 1], C = Triangles.Indices[Index + 2];
		FVector Normal = FVector::CrossProduct(
			FVector(Triangles.Positions[B] - Triangles.Positions[A]) * Scale,
			FVector(Triangles.Positions[C] - Triangles.Positions[A]) * Scale).GetSafeNormal();
		if (Triangles.Normals.Num() == Triangles.Positions.Num() &&
			FVector::DotProduct(Normal, FVector(Triangles.Normals[A] + Triangles.Normals[B] + Triangles.Normals[C])) < 0)
		{
			Normal = -Normal;
		}
		if (Transform.TransformVectorNoScale(Normal).Z >= MinZ)
		{
			// Wedges often have an exactly 45-degree LOCAL normal. CPU collision, render geometry,
			// and the shader can break that tie differently after non-uniform scaling. Supply both
			// islands; the grid still classifies each triangle once and never duplicates its area.
			const FVector LocalNormal = Normal * Scale;
			const double Largest = LocalNormal.GetAbs().GetMax();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (FMath::IsNearlyEqual(FMath::Abs(LocalNormal[Axis]), Largest, Largest * 0.0001))
				{
					Mask |= PaintDirectionBit(static_cast<EPaintFaceDirection>(Axis * 2 + (LocalNormal[Axis] < 0 ? 1 : 0)));
				}
			}
		}
	}
	return Mask;
}

void PaintPlatformCoverage::Filter(const UStaticMeshComponent& Mesh, FPaintCellGrid& Grid)
{
	UWorld* World = Mesh.GetWorld();
	if (!World) return;
	const ACharacter* Character = PlatformCharacter(World);
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float MinZ = WalkableZ(Mesh);
	const FTransform Transform(Mesh.GetComponentQuat(), Mesh.GetComponentLocation());
	FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(PaintPlatformFloor), true);
	FloorParams.MobilityType = EQueryMobilityType::Static;
	FCollisionQueryParams ClearanceParams(SCENE_QUERY_STAT(PaintPlatformClearance), false);
	ClearanceParams.MobilityType = EQueryMobilityType::Static;
	const bool bExcluded = Mesh.GetOwner() && Mesh.GetOwner()->ActorHasTag(TEXT("PaintCoverageExclude"));
	int32 Unsupported = 0, Steep = 0, Blocked = 0;
	const float Before = Grid.GetCoverage().TotalArea;
	Grid.FilterSurfaceCells([&](const FVector& Center, EPaintFaceDirection)
	{
		if (bExcluded) return false;
		const FVector Point = Transform.TransformPosition(Center);
		FHitResult Floor;
		// A local probe, not a ray from the sky: a bridge must not erase the playable floor beneath it.
		if (!World->LineTraceSingleByChannel(Floor, Point + FVector(0, 0, 2), Point - FVector(0, 0, 4),
			ECC_Visibility, FloorParams) || Floor.bStartPenetrating || Floor.GetComponent() != &Mesh)
		{
			++Unsupported;
			return false;
		}
		if (Floor.ImpactNormal.Z < MinZ || Floor.ImpactNormal.Z <= UE_KINDA_SMALL_NUMBER)
		{
			++Steep;
			return false;
		}
		// Do not ignore the support actor: another part of the same mesh can be a ceiling or an obstacle.
		const FVector CapsuleCenter = Floor.ImpactPoint + FVector(0, 0,
			CapsuleCenterHeight(Radius, HalfHeight, Floor.ImpactNormal.Z));
		if (World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(Radius, HalfHeight), ClearanceParams))
		{
			++Blocked;
			return false;
		}
		return true;
	});
	UE_LOG(LogPaint, Log, TEXT("Playable platforms %s: %.2f -> %.2f m2; unsupported=%d steep=%d blocked=%d excluded=%d"),
		*Mesh.GetReadableName(), Before / 10000, Grid.GetCoverage().TotalArea / 10000, Unsupported, Steep, Blocked, bExcluded);
}
