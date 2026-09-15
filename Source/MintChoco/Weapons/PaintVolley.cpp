#include "Weapons/PaintVolley.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Weapons/PaintballProfile.h"

APaintVolley::APaintVolley()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

APaintVolley* APaintVolley::Spawn(UWorld& World, const FPaintVolleyParams& InParams, APawn* Shooter,
	TSubclassOf<APaintVolley> Class)
{
	if (!InParams.Paintball || InParams.Count <= 0 || World.GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	const FTransform Transform(FVector(InParams.Origin));
	APaintVolley* const Volley = World.SpawnActorDeferred<APaintVolley>(
		Class ? Class.Get() : StaticClass(), Transform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Volley)
	{
		return nullptr;
	}

	Volley->Params = InParams;
	if (Volley->Params.Seed == 0)
	{
		Volley->Params.Seed = FMath::Rand();
	}
	Volley->FinishSpawning(Transform);
	return Volley;
}

void APaintVolley::BeginPlay()
{
	Super::BeginPlay();
	// 마지막 탄이 떠난 뒤에도 잠시 남는다. 탄은 자기 수명대로 산다.
	SetLifeSpan(static_cast<float>(Params.Count) * Params.Interval + 2.0f);
	Start();
}

void APaintVolley::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(APaintVolley, Params, COND_InitialOnly);
}

void APaintVolley::OnRep_Params()
{
	if (HasActorBegunPlay())
	{
		Start();
	}
}

void APaintVolley::Start()
{
	if (bStarted || !Params.Paintball || Params.Count <= 0)
	{
		return;
	}
	bStarted = true;
	NextIndex = 0;

	// 첫 발은 기다리지 않는다. 방아쇠를 당긴 순간 총구에서 무언가 나가야 한다.
	FireNext();
	if (Params.Count > 1)
	{
		GetWorldTimerManager().SetTimer(ShotTimer, this, &APaintVolley::FireNext, FMath::Max(Params.Interval, 0.01f), /*bLoop=*/true);
	}
}

void APaintVolley::FireNext()
{
	UWorld* const World = GetWorld();
	if (!World || !Params.Paintball || NextIndex >= Params.Count)
	{
		GetWorldTimerManager().ClearTimer(ShotTimer);
		return;
	}

	const int32 Index = NextIndex++;
	const FVector Muzzle(Params.Origin);
	const FVector Aim(Params.Direction);

	// 모든 탄이 **조준선 그대로** 나간다. 눈에 보이는 궤적선이 선두이고 탄들이 그 뒤를
	// 따르므로, 보이는 선과 실제로 날아가는 곳이 어긋나지 않는다.
	const FVector Velocity = Aim * Params.Speed;

	// 순서는 “언제 꺾이느냐” 로 만든다. n 번째 탄은 Spacing × n 만큼 직진한 뒤 흘러내리므로,
	// 같은 속도로 한 줄기를 이루다가 앞선 것부터 차례로 아래로 빠진다.
	const float TargetDistance = Params.Spacing * static_cast<float>(Index + 1);
	const float StraightDistance = FMath::Max(TargetDistance - Params.DropLead, 0.0f);
	const float StraightTime = StraightDistance / FMath::Max(Params.Speed, 1.0f);

	const int32 BallSeed = static_cast<int32>(
		HashCombineFast(static_cast<uint32>(Params.Seed), static_cast<uint32>(Index)));

	// 서버의 탄이 칠하고, 클라이언트의 탄은 같은 자리의 그림이다. 꺾이는 시각은 양쪽이
	// 같은 식으로 계산하므로 복제할 것이 없다.
	Params.Paintball->Launch(*World, FTransform(Velocity.Rotation(), Muzzle), GetInstigator(),
		Velocity, Params.PaintId, BallSeed, /*bCosmetic=*/!HasAuthority(), StraightTime);

	if (NextIndex >= Params.Count)
	{
		GetWorldTimerManager().ClearTimer(ShotTimer);
	}
}
