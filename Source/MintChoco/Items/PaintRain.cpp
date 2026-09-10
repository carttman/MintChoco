#include "Items/PaintRain.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Weapons/PaintballProfile.h"

// ---------------------------------------------------------------- FPaintRainPlan

int32 FPaintRainPlan::CountRows(const FVector2D& Origin, const FVector2D& Direction, const FBox2D& Bounds, float Spacing, int32 MaxRows)
{
	const FVector2D Step = Direction.GetSafeNormal() * FMath::Max(Spacing, 1.0f);
	if (Step.IsNearlyZero())
	{
		return 0;
	}

	int32 Last = 0;
	for (int32 Row = 1; Row <= MaxRows; ++Row)
	{
		const FVector2D Point = Origin + Step * static_cast<float>(Row);
		// 경계선 위의 점도 맵이다(IsInside는 경계를 뺀다).
		const bool bInside = Point.X >= Bounds.Min.X && Point.X <= Bounds.Max.X && Point.Y >= Bounds.Min.Y && Point.Y <= Bounds.Max.Y;
		if (bInside)
		{
			Last = Row;
		}
	}
	return Last;
}

FVector FPaintRainPlan::RowPoint(const FPaintRainParams& Params, int32 Row, int32 Column)
{
	const FVector2D Forward = Params.Direction.GetSafeNormal();
	const FVector2D Right(-Forward.Y, Forward.X);
	const float Lateral = (static_cast<float>(Column) - static_cast<float>(Params.Columns - 1) * 0.5f) * Params.ColumnSpacing;
	const FVector2D Point = FVector2D(Params.Origin.X, Params.Origin.Y) + Forward * (Params.RowSpacing * static_cast<float>(Row)) + Right * Lateral;
	return FVector(Point.X, Point.Y, Params.DropZ);
}

// ---------------------------------------------------------------- APaintRain

APaintRain::APaintRain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

APaintRain* APaintRain::Spawn(UWorld& World, const FPaintRainParams& InParams, TSubclassOf<APaintRain> Class)
{
	if (!InParams.Paintball || InParams.RowCount <= 0 || World.GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	const FTransform Transform(FVector(InParams.Origin));
	APaintRain* const Rain = World.SpawnActorDeferred<APaintRain>(
		Class ? Class.Get() : StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Rain)
	{
		return nullptr;
	}
	Rain->Params = InParams;
	if (Rain->Params.Seed == 0)
	{
		Rain->Params.Seed = FMath::Rand();
	}
	Rain->FinishSpawning(Transform);
	return Rain;
}

void APaintRain::BeginPlay()
{
	Super::BeginPlay();
	// 마지막 행이 떨어진 뒤에도 잠시 남는다. 탄은 자기 수명대로 산다.
	SetLifeSpan(static_cast<float>(Params.RowCount) * Params.Interval + 2.0f);
	Start();
}

void APaintRain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(APaintRain, Params, COND_InitialOnly);
}

void APaintRain::OnRep_Params()
{
	if (HasActorBegunPlay())
	{
		Start();
	}
}

void APaintRain::Start()
{
	if (bStarted || !Params.Paintball || Params.RowCount <= 0)
	{
		return;
	}
	bStarted = true;
	NextRow = 1;
	DropRow();
	if (Params.RowCount > 1)
	{
		GetWorldTimerManager().SetTimer(RowTimer, this, &APaintRain::DropRow, FMath::Max(Params.Interval, 0.01f), /*bLoop=*/true);
	}
}

void APaintRain::DropRow()
{
	UWorld* const World = GetWorld();
	if (!World || !Params.Paintball || NextRow > Params.RowCount)
	{
		GetWorldTimerManager().ClearTimer(RowTimer);
		return;
	}

	// 서버의 탄이 칠하고, 클라이언트의 탄은 같은 자리의 그림이다.
	const bool bCosmetic = !HasAuthority();
	const FVector Velocity(0.0f, 0.0f, -Params.DropSpeed);
	for (int32 Column = 0; Column < Params.Columns; ++Column)
	{
		const FVector Point = FPaintRainPlan::RowPoint(Params, NextRow, Column);
		const int32 BallSeed = static_cast<int32>(HashCombineFast(static_cast<uint32>(Params.Seed), static_cast<uint32>(NextRow * 16 + Column)));
		Params.Paintball->Launch(*World, FTransform(FRotator(-90.0f, 0.0f, 0.0f), Point), nullptr, Velocity, Params.PaintId, BallSeed, bCosmetic);
	}

	++NextRow;
	if (NextRow > Params.RowCount)
	{
		GetWorldTimerManager().ClearTimer(RowTimer);
	}
}
