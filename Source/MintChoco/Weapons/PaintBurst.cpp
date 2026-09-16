#include "Weapons/PaintBurst.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Game/TeamLook.h"
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

float PaintBurst::SpeedForRange(float RangeCm, float GravityScale)
{
	// 980 cm/s²가 기본 중력. 반경 300에 중력 0.5면 383 cm/s가 나온다.
	return FMath::Sqrt(FMath::Max(RangeCm, 0.0f) * 980.0f * FMath::Max(GravityScale, UE_KINDA_SMALL_NUMBER));
}

void PaintBurst::ComputeScatterOffsets(int32 Seed, int32 Count, float Radius, TArray<FVector2D>& OutOffsets)
{
	OutOffsets.Reset(Count);
	const FRandomStream Random(Seed);
	const float Safe = FMath::Max(Radius, 0.0f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Yaw = (static_cast<float>(Index) + Random.FRand()) * (360.0f / FMath::Max(Count, 1));
		// √로 펴지 않으면 면적이 반경의 제곱으로 늘어나는 만큼 가운데가 몰린다.
		const float Distance = Safe * FMath::Sqrt(Random.FRand());
		const float Radians = FMath::DegreesToRadians(Yaw);
		OutOffsets.Add(FVector2D(FMath::Cos(Radians), FMath::Sin(Radians)) * Distance);
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

	SpawnBurstFX();

	// 서버의 탄이 칠하고, 클라이언트의 탄은 같은 궤적의 그림이다.
	const bool bCosmetic = !HasAuthority();

	if (Params.ScatterRadius > 0.0f)
	{
		BurstScatter(bCosmetic);
	}
	else
	{
		BurstRadial(bCosmetic);
	}
}

void APaintBurst::BurstRadial(bool bCosmetic)
{
	UWorld* const World = GetWorld();

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

void APaintBurst::BurstScatter(bool bCosmetic)
{
	UWorld* const World = GetWorld();

	TArray<FVector2D> Offsets;
	PaintBurst::ComputeScatterOffsets(Params.Seed, Params.Count, Params.ScatterRadius, Offsets);

	const FVector Origin = GetActorLocation();
	const float Gravity = Params.Paintball->GravityScale;
	for (int32 Index = 0; Index < Offsets.Num(); ++Index)
	{
		const int32 BallSeed = static_cast<int32>(HashCombineFast(static_cast<uint32>(Params.Seed), static_cast<uint32>(Index)));

		// 45도로 던지면 사거리가 곧 v²/(980×중력)이라, 착탄점까지의 거리만으로 속도가 나온다.
		// 가운데에 떨어질 탄은 거리가 0이라 속도도 0이고, 그 자리에서 그대로 떨어진다.
		const float Distance = static_cast<float>(Offsets[Index].Size());
		const float Speed = PaintBurst::SpeedForRange(Distance, Gravity);
		const FVector Flat(Offsets[Index].X, Offsets[Index].Y, 0.0f);
		const FVector Direction = (Flat.GetSafeNormal() + FVector::UpVector).GetSafeNormal();

		const FTransform SpawnTransform(Direction.Rotation(), Origin);
		Params.Paintball->Launch(*World, SpawnTransform, /*Instigator=*/nullptr, Direction * Speed, Params.PaintId, BallSeed, bCosmetic);
	}
}

void APaintBurst::SpawnBurstFX()
{
	UWorld* const World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 액터가 복제되어 머신마다 한 번 BeginPlay를 지나므로 소리도 여기서 한 번이다. 뱅크가 실려
	// 왔으면 그 아이템의 파열음이, 비어 있으면 기본 뱅크의 소리가 난다.
	UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_World_Burst, GetActorLocation(), Params.Sounds);

	if (!Params.BurstFX)
	{
		return;
	}

	// 회전을 주지 않는다. 이 연출은 밑동이 바닥에 놓인 물기둥이라 늘 월드 위로 솟아야 하는데,
	// 히트 노멀을 따르게 하면 벽에서 터졌을 때 기둥이 벽을 뚫고 옆으로 눕는다. 꿀풍선은
	// 벽에 맞아도 터지므로(AItemProjectile::HandleWorldHit) 그 경우가 실제로 나온다.
	if (UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, Params.BurstFX, GetActorLocation(), FRotator::ZeroRotator, FVector(Params.BurstFXScale)))
	{
		FX->SetVariableLinearColor(TeamLook::NiagaraTintParameter, TeamLook::GetColor(Params.PaintId, World));
	}
}
