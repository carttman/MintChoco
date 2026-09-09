#include "Weapons/PaintBurst.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include "Weapons/PaintballProfile.h"

void PaintBurst::ComputeDirections(int32 Seed, int32 Count, float MinPitchDeg, float MaxPitchDeg, TArray<FVector>& OutDirections)
{
	OutDirections.Reset(Count);
	const FRandomStream Random(Seed);
	const float Low = FMath::Min(MinPitchDeg, MaxPitchDeg);
	const float High = FMath::Max(MinPitchDeg, MaxPitchDeg);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Yaw = (static_cast<float>(Index) + Random.FRand()) * (360.0f / FMath::Max(Count, 1));
		const float Pitch = Random.FRandRange(Low, High);
		OutDirections.Add(FRotator(Pitch, Yaw, 0.0f).Vector());
	}
}

APaintBurst::APaintBurst()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// 한 번의 넷 틱은 살아 있어야 클라이언트에 닿는다. 탄은 자기 수명대로 산다.
	InitialLifeSpan = 2.0f;
}

APaintBurst* APaintBurst::Spawn(UWorld& World, const FVector& Location, const FPaintBurstParams& InParams, TSubclassOf<APaintBurst> Class)
{
	if (!InParams.Paintball || World.GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APaintBurst* const Burst = World.SpawnActorDeferred<APaintBurst>(
		Class ? Class.Get() : StaticClass(), FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Burst)
	{
		return nullptr;
	}
	Burst->Params = InParams;
	if (Burst->Params.Seed == 0)
	{
		Burst->Params.Seed = FMath::Rand();
	}
	Burst->FinishSpawning(FTransform(Location));
	return Burst;
}

void APaintBurst::BeginPlay()
{
	Super::BeginPlay();
	Burst();
}

void APaintBurst::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(APaintBurst, Params, COND_InitialOnly);
}

void APaintBurst::OnRep_Params()
{
	// 보통은 BeginPlay 전에 와서 아무 일도 없다. 프로필이 늦게 매핑된 경우만 여기서 터진다.
	if (HasActorBegunPlay())
	{
		Burst();
	}
}

void APaintBurst::Burst()
{
	UWorld* const World = GetWorld();
	if (bBurst || !World || !Params.Paintball)
	{
		return;
	}
	bBurst = true;

	// 서버의 탄이 칠하고, 클라이언트의 탄은 같은 궤적의 그림이다.
	const bool bCosmetic = !HasAuthority();

	TArray<FVector> Directions;
	PaintBurst::ComputeDirections(Params.Seed, Params.Count, Params.MinPitch, Params.MaxPitch, Directions);

	const FVector Origin = GetActorLocation();
	for (int32 Index = 0; Index < Directions.Num(); ++Index)
	{
		const int32 BallSeed = static_cast<int32>(HashCombineFast(static_cast<uint32>(Params.Seed), static_cast<uint32>(Index)));
		const FTransform SpawnTransform(Directions[Index].Rotation(), Origin);
		Params.Paintball->Launch(*World, SpawnTransform, /*Instigator=*/nullptr, Directions[Index] * Params.Speed, Params.PaintId, BallSeed, bCosmetic);
	}
}
