#include "Items/ItemAimPreview.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AItemAimPreview::AItemAimPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Dots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Dots"));
	SetRootComponent(Dots);
	Dots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dots->SetGenerateOverlapEvents(false);
	Dots->SetCastShadow(false);
	// 조준 중에만 있는 표시라 라이팅에 끼어들 이유가 없다.
	Dots->bAffectDistanceFieldLighting = false;
	Dots->SetMobility(EComponentMobility::Movable);

	// 꿀풍선의 구가 막히는 것들: 월드 지오메트리와 폰. 폰은 실제로는 겹침으로 터지지만,
	// 곡선을 거기서 끊어야 표시된 착탄점이 맞는다.
	BlockingObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	BlockingObjects.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	BlockingObjects.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

AItemAimPreview* AItemAimPreview::Spawn(UWorld& World, AActor* Owner, const FItemAimPreviewStyle& InStyle)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Owner;

	AItemAimPreview* const Preview = World.SpawnActor<AItemAimPreview>(
		AItemAimPreview::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Preview)
	{
		return nullptr;
	}

	Preview->Style = InStyle;
	Preview->SetActorEnableCollision(false);

	if (InStyle.DotMesh && Preview->Dots)
	{
		Preview->Dots->SetStaticMesh(InStyle.DotMesh);
		if (InStyle.DotMaterial)
		{
			Preview->Dots->SetMaterial(0, InStyle.DotMaterial);
		}
	}

	return Preview;
}

void AItemAimPreview::UpdateArc(const FVector& Start, const FVector& Velocity, float GravityScale, float Radius, const AActor* IgnoreActor)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FPredictProjectilePathParams Params(Radius, Start, Velocity, Style.MaxSimTime);
	Params.bTraceWithCollision = true;
	Params.bTraceComplex = false;
	Params.bTraceWithChannel = false;
	Params.ObjectTypes = BlockingObjects;
	Params.SimFrequency = 20.0f;
	if (IgnoreActor)
	{
		Params.ActorsToIgnore.Add(const_cast<AActor*>(IgnoreActor));
	}
	// 0은 "덮어쓰지 않는다"는 뜻이라 중력 0을 그대로 넣을 수 없다. 아주 작은 값이면 직선과 같다.
	Params.OverrideGravityZ = FMath::IsNearlyZero(GravityScale)
		? -UE_KINDA_SMALL_NUMBER
		: World->GetGravityZ() * GravityScale;

	FPredictProjectilePathResult Result;
	const bool bHit = UGameplayStatics::PredictProjectilePath(World, Params, Result);

	const FVector Impact = bHit ? Result.HitResult.ImpactPoint : Result.LastTraceDestination.Location;
	UpdateImpactMarker(Impact, bHit);

	if (Result.PathData.Num() < 2)
	{
		if (Dots)
		{
			Dots->ClearInstances();
		}
		return;
	}

	// 점을 찍을 메시가 없다. 개발 빌드에서는 선이라도 보여 준다.
	if (!Style.DotMesh || !Dots || !Dots->GetStaticMesh())
	{
#if ENABLE_DRAW_DEBUG
		for (int32 Index = 1; Index < Result.PathData.Num(); ++Index)
		{
			DrawDebugLine(World, Result.PathData[Index - 1].Location, Result.PathData[Index].Location,
				FColor::Yellow, /*bPersistentLines=*/false, /*LifeTime=*/-1.0f, /*DepthPriority=*/0, /*Thickness=*/2.0f);
		}
#endif
		return;
	}

	// 궤적을 따라 잰 거리로 고르게 찍는다. 시뮬레이션 점은 시간 간격이라 빠른 구간이 성글다.
	const float MeshSize = FMath::Max(Style.DotMesh->GetBounds().BoxExtent.GetMax() * 2.0f, 1.0f);
	const FVector DotScale(Style.DotSize / MeshSize);
	const float Spacing = FMath::Max(Style.DotSpacing, 5.0f);

	DotTransforms.Reset();
	float DistanceToNextDot = 0.0f;
	for (int32 Index = 1; Index < Result.PathData.Num(); ++Index)
	{
		const FVector From = Result.PathData[Index - 1].Location;
		const FVector To = Result.PathData[Index].Location;
		const float SegmentLength = FVector::Dist(From, To);
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Direction = (To - From) / SegmentLength;
		float Along = DistanceToNextDot;
		for (; Along < SegmentLength; Along += Spacing)
		{
			DotTransforms.Emplace(FRotator::ZeroRotator, From + Direction * Along, DotScale);
		}

		// 이번 구간을 넘어간 만큼이 다음 구간의 첫 점까지의 거리다. 그래야 구간 경계에서
		// 간격이 무너지지 않는다.
		DistanceToNextDot = Along - SegmentLength;
	}

	// 개수가 자주 바뀌므로 통째로 다시 넣는다. 점은 많아야 수십 개다.
	Dots->ClearInstances();
	if (DotTransforms.Num() > 0)
	{
		Dots->AddInstances(DotTransforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
	}
}

void AItemAimPreview::UpdateImpactMarker(const FVector& Location, bool bHit)
{
	UWorld* const World = GetWorld();
	if (!Style.ImpactMarkerClass || !World)
	{
		return;
	}

	if (!ImpactMarker)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = GetOwner();
		ImpactMarker = World->SpawnActor<AActor>(Style.ImpactMarkerClass, Location, FRotator::ZeroRotator, Params);
		if (!ImpactMarker)
		{
			return;
		}
		ImpactMarker->SetActorEnableCollision(false);
	}

	ImpactMarker->SetActorLocation(Location);
	ImpactMarker->SetActorHiddenInGame(!bHit);
}

void AItemAimPreview::Destroyed()
{
	if (ImpactMarker)
	{
		ImpactMarker->Destroy();
		ImpactMarker = nullptr;
	}

	Super::Destroyed();
}
