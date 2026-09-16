#include "Items/PaintRain.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

#include "Game/TeamLook.h"
#include "Weapons/PaintballProfile.h"

namespace
{
	/**
	 * 예고 표식이 지면을 찾아 내려가는 거리(cm). 탄이 태어나는 높이는 맵 경계 최고점 위라,
	 * 맵 어디서든 바닥에 닿고도 남을 만큼 넉넉히 잡는다. 못 찾으면 그 행은 건너뛴다.
	 */
	constexpr float TelegraphTraceReach = 100000.0f;
}

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

int32 FPaintRainPlan::TelegraphCount(int32 RowCount, int32 Stride)
{
	if (RowCount <= 0)
	{
		return 0;
	}
	// 첫 행에는 언제나 하나 놓이므로 나눠 올린다. 나누어떨어지지 않는 마지막 구간도 표식을 받는다.
	const int32 Step = FMath::Max(Stride, 1);
	return (RowCount + Step - 1) / Step;
}

float FPaintRainPlan::TelegraphInterval(float LeadInSeconds, int32 TelegraphCount)
{
	if (LeadInSeconds <= 0.0f || TelegraphCount <= 0)
	{
		return 0.0f;
	}
	return LeadInSeconds / static_cast<float>(TelegraphCount);
}

float FPaintRainPlan::Lifespan(const FPaintRainParams& Params)
{
	return FMath::Max(Params.LeadInSeconds, 0.0f) + static_cast<float>(Params.RowCount) * Params.Interval + 2.0f;
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
	SetLifeSpan(FPaintRainPlan::Lifespan(Params));
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

	// 예고는 리드인 동안만 돈다. 간격이 0이면(리드인이 없으면) 표식도 없다.
	const int32 TelegraphMarks = FPaintRainPlan::TelegraphCount(Params.RowCount, Params.TelegraphRowStride);
	const float TelegraphStep = FPaintRainPlan::TelegraphInterval(Params.LeadInSeconds, TelegraphMarks);
	if (TelegraphStep > 0.0f && Params.TelegraphFX && GetNetMode() != NM_DedicatedServer)
	{
		NextTelegraphRow = 1;
		PlaceTelegraph();
		if (TelegraphMarks > 1)
		{
			GetWorldTimerManager().SetTimer(TelegraphTimer, this, &APaintRain::PlaceTelegraph, TelegraphStep, /*bLoop=*/true);
		}
	}

	// 리드인이 있으면 그만큼 기다렸다가 첫 행을 떨어뜨린다. 파라미터가 초기 복제로 오므로
	// 모든 머신이 같은 시점에 시작한다.
	if (Params.LeadInSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RowTimer, this, &APaintRain::BeginRows, Params.LeadInSeconds, /*bLoop=*/false);
		return;
	}

	BeginRows();
}

void APaintRain::BeginRows()
{
	DropRow();
	if (Params.RowCount > 1)
	{
		GetWorldTimerManager().SetTimer(RowTimer, this, &APaintRain::DropRow, FMath::Max(Params.Interval, 0.01f), /*bLoop=*/true);
	}
}

void APaintRain::PlaceTelegraph()
{
	UWorld* const World = GetWorld();
	if (!World || NextTelegraphRow > Params.RowCount)
	{
		GetWorldTimerManager().ClearTimer(TelegraphTimer);
		return;
	}

	FVector Ground;
	if (FindGroundAtRow(NextTelegraphRow, Ground))
	{
		if (UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, Params.TelegraphFX, Ground, FRotator::ZeroRotator))
		{
			FX->SetVariableLinearColor(TeamLook::NiagaraTintParameter, TeamLook::GetColor(Params.PaintId, World));
		}
	}

	NextTelegraphRow += FMath::Max(Params.TelegraphRowStride, 1);
	if (NextTelegraphRow > Params.RowCount)
	{
		GetWorldTimerManager().ClearTimer(TelegraphTimer);
	}
}

bool APaintRain::FindGroundAtRow(int32 Row, FVector& OutPoint) const
{
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 가운데 열이 중심선이다. 탄이 태어나는 높이에서 곧장 내려다본다.
	const FVector From = FPaintRainPlan::RowPoint(Params, Row, Params.Columns / 2);
	const FVector To = From - FVector(0.0f, 0.0f, TelegraphTraceReach);

	// 스피드 스타 자국이 바닥을 찾는 방식과 같은 채널이다.
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BombardmentTelegraph), /*bTraceComplex=*/true);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query))
	{
		return false;
	}

	OutPoint = Hit.ImpactPoint;
	return true;
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
