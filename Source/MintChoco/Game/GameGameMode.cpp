// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/GameGameMode.h"

#include "EngineUtils.h"
#include "Game/GameGameState.h"
#include "Game/GamePlayerState.h"
#include "Game/TeamPlayerStart.h"
#include "Game/Unit.h"
#include "Game/UnitDataAsset.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Items/ItemPickup.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "Items/ItemSpawnPoint.h"
#include "Kismet/GameplayStatics.h"
#include "MintChoco.h"
#include "TimerManager.h"

AGameGameMode::AGameGameMode()
{
	PlayerStateClass = AGamePlayerState::StaticClass();
	GameStateClass = AGameGameState::StaticClass();
}

void AGameGameMode::StartPlay()
{
	Super::StartPlay();

	StartItemSpawning();

	if (MatchDuration <= 0.0f)
	{
		return;
	}

	AGameGameState* const State = GetGameState<AGameGameState>();
	if (!State)
	{
		UE_LOG(LogMintChoco, Warning,
			TEXT("AGameGameMode::StartPlay: GameState가 AGameGameState가 아니라 경기 타이머를 걸지 못했다."));
		return;
	}

	// 남은 초가 아니라 끝나는 시각만 복제한다. 클라이언트는 GetServerWorldTimeSeconds()로
	// 남은 시간을 직접 계산하므로 복제는 이 한 번이면 된다.
	State->SetMatchEndTime(State->GetServerWorldTimeSeconds() + MatchDuration);

	GetWorldTimerManager().SetTimer(
		MatchTimer, this, &AGameGameMode::OnMatchTimeExpired, MatchDuration, /*bLoop=*/false);
}

void AGameGameMode::StartItemSpawning()
{
	ItemSpawnPoints.Reset();
	for (TActorIterator<AItemSpawnPoint> It(GetWorld()); It; ++It)
	{
		ItemSpawnPoints.Add(*It);
	}

	TArray<UItemProfile*> Items;
	UItemSettings::Get().LoadItems(Items);

	if (ItemSpawnInterval <= 0.0f || ItemSpawnPoints.IsEmpty() || Items.IsEmpty() || !UItemSettings::Get().LoadPickupClass())
	{
		UE_LOG(LogMintChoco, Log,
			TEXT("아이템 스폰 없음: 주기 %.1f초, 스폰 지점 %d개, 아이템 %d종, 픽업 클래스 %s."),
			ItemSpawnInterval, ItemSpawnPoints.Num(), Items.Num(),
			UItemSettings::Get().PickupClass.IsNull() ? TEXT("없음") : TEXT("있음"));
		return;
	}

	ItemRandom.GenerateNewSeed();

	// 예고가 주기 안에 들어가야 첫 아이템이 정확히 한 주기 뒤에 나온다.
	const float Warning = FMath::Clamp(ItemSpawnWarning, 0.0f, ItemSpawnInterval);
	const float FirstDelay = FMath::Max(ItemSpawnInterval - Warning, UE_KINDA_SMALL_NUMBER);
	GetWorldTimerManager().SetTimer(ItemSpawnTimer, this, &AGameGameMode::SpawnNextItem, ItemSpawnInterval, /*bLoop=*/true, FirstDelay);
}

int32 AGameGameMode::PickFreeSpawnIndex(const TArray<bool>& bFree, const FRandomStream& Random)
{
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < bFree.Num(); ++Index)
	{
		if (bFree[Index])
		{
			Candidates.Add(Index);
		}
	}
	if (Candidates.IsEmpty())
	{
		return INDEX_NONE;
	}
	return Candidates[Random.RandRange(0, Candidates.Num() - 1)];
}

void AGameGameMode::SpawnNextItem()
{
	TArray<bool> bFree;
	bFree.Reserve(ItemSpawnPoints.Num());
	for (const AItemSpawnPoint* const Point : ItemSpawnPoints)
	{
		bFree.Add(Point && Point->IsFree());
	}

	const int32 PointIndex = PickFreeSpawnIndex(bFree, ItemRandom);
	if (PointIndex == INDEX_NONE)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("아이템 스폰 건너뜀: 모든 지점이 차 있다."));
		return;
	}

	TArray<UItemProfile*> Items;
	UItemSettings::Get().LoadItems(Items);
	UClass* const PickupClass = UItemSettings::Get().LoadPickupClass();
	if (Items.IsEmpty() || !PickupClass)
	{
		return;
	}

	AItemSpawnPoint* const Point = ItemSpawnPoints[PointIndex];
	UItemProfile* const Item = Items[ItemRandom.RandRange(0, Items.Num() - 1)];

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AItemPickup* const Pickup = GetWorld()->SpawnActorDeferred<AItemPickup>(
		PickupClass, Point->GetActorTransform(), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}

	const float Warning = FMath::Clamp(ItemSpawnWarning, 0.0f, ItemSpawnInterval);
	Pickup->Initialize(Item, Point, Warning);
	Pickup->FinishSpawning(Point->GetActorTransform());

	UE_LOG(LogMintChoco, Verbose, TEXT("아이템 예고: %s at %s, %.1f초 뒤 등장."), *GetNameSafe(Item), *GetNameSafe(Point), Warning);
}

void AGameGameMode::OnMatchTimeExpired()
{
	// 끝난 경기에 아이템이 계속 나올 이유가 없다. 이미 놓인 것은 그대로 둔다.
	GetWorldTimerManager().ClearTimer(ItemSpawnTimer);

	AGameGameState* const State = GetGameState<AGameGameState>();
	if (!State)
	{
		return;
	}

	// 이 값으로 승패가 갈리므로 마지막으로 한 번 다시 잰다. 주기 갱신값은 최대
	// CoverageRefreshInterval만큼 낡아 있을 수 있다.
	State->RefreshCoverage();
	const FPaintCoverage& Coverage = State->GetWorldCoverage();

	// PaintId는 팀 번호를 그대로 쓴다(Unit.cpp의 SetPaintId).
	// 1위와 2위를 함께 찾아 두 값의 차이로 판정한다. 팀이 셋 이상이 되어도 그대로 성립한다.
	int32 BestTeam = Teams::None;
	float BestFraction = -1.0f;
	float SecondFraction = -1.0f;

	for (int32 Team = 0; Team < Teams::Count; ++Team)
	{
		const float Fraction = Coverage.GetFraction(static_cast<uint8>(Team));
		if (Fraction > BestFraction)
		{
			SecondFraction = BestFraction;
			BestFraction = Fraction;
			BestTeam = Team;
		}
		else if (Fraction > SecondFraction)
		{
			SecondFraction = Fraction;
		}
	}

	// 팀이 하나뿐인 구성에서도 안전하도록 2위를 0으로 바닥 처리한다.
	SecondFraction = FMath::Max(SecondFraction, 0.0f);

	// 칠해진 양 중 1위가 얼마나 앞섰는지. 맵 전체가 아니라 두 팀의 합으로 나누므로,
	// 맵이 거의 비어 있어도 접전과 압승이 구분된다.
	// 아무도 칠하지 않았으면 격차를 잴 수 없고, 그 경우도 무승부다.
	const float PaintedFraction = BestFraction + SecondFraction;
	const float RelativeMargin = PaintedFraction > 0.0f
		? (BestFraction - SecondFraction) / PaintedFraction
		: 0.0f;

	const bool bDraw = RelativeMargin <= DrawMarginFraction;
	const int32 Winner = bDraw ? Teams::None : BestTeam;

	State->SetMatchResult(Winner);

	UE_LOG(LogMintChoco, Log,
		TEXT("경기 종료: %s (1위 %.2f%% vs 2위 %.2f%%, 상대 격차 %.1f%% / 무승부 기준 %.1f%%) | %s"),
		bDraw ? TEXT("무승부") : Teams::GetDisplayName(Winner),
		BestFraction * 100.0f,
		SecondFraction * 100.0f,
		RelativeMargin * 100.0f,
		DrawMarginFraction * 100.0f,
		*Coverage.ToString());
}

int32 AGameGameMode::GetTeamOf(const AController* Player) const
{
	const AGamePlayerState* PlayerState = Player ? Player->GetPlayerState<AGamePlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetTeam() : Teams::None;
}

AActor* AGameGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const int32 Team = GetTeamOf(Player);

	TArray<APlayerStart*> TeamStarts;
	TArray<APlayerStart*> NeutralStarts;
	GatherPlayerStarts(Team, TeamStarts, NeutralStarts);

	// 1순위: 자기 팀 지점 중 비어 있고 적에게서 먼 곳.
	if (APlayerStart* Chosen = PickFreeStart(TeamStarts, Team))
	{
		return Chosen;
	}

	// 2순위: 중립 지점. 팀 지점을 아직 배치하지 않은 맵이 여기로 떨어진다.
	if (APlayerStart* Chosen = PickFreeStart(NeutralStarts, Team))
	{
		return Chosen;
	}

	// 3순위: 전부 막혔으면 겹쳐서라도 자기 팀 지점에서 나온다.
	// 스폰 실패로 관전 상태에 묶이는 것이 잠깐 겹치는 것보다 훨씬 나쁘다.
	if (TeamStarts.Num() > 0)
	{
		return TeamStarts[FMath::RandRange(0, TeamStarts.Num() - 1)];
	}

	if (NeutralStarts.Num() > 0)
	{
		return NeutralStarts[FMath::RandRange(0, NeutralStarts.Num() - 1)];
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* AGameGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass)
	{
		return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// FinishSpawning 전에 UnitData를 넣기 위해 지연 스폰한다. 이렇게 해야
	// AUnit::PostInitializeComponents의 ApplyUnitData()가 처음부터 올바른 값을 보고,
	// 블루프린트 기본 메시가 한 프레임 보였다가 바뀌는 깜빡임이 생기지 않는다.
	SpawnInfo.bDeferConstruction = true;

	APawn* Pawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (!Pawn)
	{
		return nullptr;
	}

	ApplyTeamUnitData(Pawn, NewPlayer);

	UGameplayStatics::FinishSpawningActor(Pawn, SpawnTransform);
	return Pawn;
}

void AGameGameMode::ApplyTeamUnitData(APawn* Pawn, const AController* NewPlayer) const
{
	AUnit* Unit = Cast<AUnit>(Pawn);
	if (!Unit)
	{
		// 이 주입 방식은 폰이 AUnit일 때만 성립한다. DefaultPawnClass가 다른 폰이면
		// 팀 설정이 아무리 맞아도 조용히 무시되므로 여기서 드러낸다.
		UE_LOG(LogMintChoco, Warning,
			TEXT("%s: DefaultPawnClass가 AUnit이 아니라 %s입니다. 팀별 캐릭터가 적용되지 않습니다."),
			*GetNameSafe(NewPlayer), *GetNameSafe(Pawn ? Pawn->GetClass() : nullptr));
		return;
	}

	const int32 Team = GetTeamOf(NewPlayer);

	if (UUnitDataAsset* Data = FindUnitDataForTeam(Team))
	{
		Unit->SetUnitData(Data);
	}
	else if (Teams::IsValidId(Team))
	{
		// 설정을 빠뜨리면 폰 블루프린트의 기본값으로 양 팀이 같은 캐릭터로 나온다.
		UE_LOG(LogMintChoco, Warning,
			TEXT("%s: 팀 %d의 UnitData가 없습니다. BP_GameMode의 Team Unit Data를 확인하세요."),
			*GetNameSafe(NewPlayer), Team);
	}
	else
	{
		// 로비에서 팀을 고르지 않았거나 ReceiveCopyProperties가 값을 옮기지 못한 경우다.
		// Team의 기본값이 0이던 시절에는 이 상황이 조용히 민트로 둔갑해 드러나지 않았다.
		UE_LOG(LogMintChoco, Warning,
			TEXT("%s: 팀이 정해지지 않은 채 스폰됐습니다(Team=%d). 로비의 팀 선택과 "
				 "BP_LobbyPlayerState의 ReceiveCopyProperties를 확인하세요."),
			*GetNameSafe(NewPlayer), Team);
	}
}

UUnitDataAsset* AGameGameMode::FindUnitDataForTeam(int32 Team) const
{
	return TeamUnitData.IsValidIndex(Team) ? TeamUnitData[Team].Get() : nullptr;
}

void AGameGameMode::GatherPlayerStarts(int32 Team, TArray<APlayerStart*>& OutTeamStarts, TArray<APlayerStart*>& OutNeutralStarts) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsValid(Start))
		{
			continue;
		}

		const ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);

		// 팀이 지정되지 않은 평범한 PlayerStart는 중립으로 취급한다.
		// 덕분에 팀 지점을 아직 배치하지 않은 맵도 그대로 동작한다.
		if (!TeamStart || TeamStart->Team == Teams::None)
		{
			OutNeutralStarts.Add(Start);
		}
		else if (TeamStart->Team == Team)
		{
			OutTeamStarts.Add(Start);
		}
	}
}

APlayerStart* AGameGameMode::PickFreeStart(const TArray<APlayerStart*>& Candidates, int32 Team) const
{
	TArray<TPair<float, APlayerStart*>> Scored;
	Scored.Reserve(Candidates.Num());

	for (APlayerStart* Start : Candidates)
	{
		if (IsStartOccupied(Start))
		{
			continue;
		}

		Scored.Emplace(DistanceToNearestEnemy(Start, Team), Start);
	}

	if (Scored.IsEmpty())
	{
		return nullptr;
	}

	// 적에게서 먼 순으로 정렬한 뒤 상위 몇 개 중에서 무작위로 고른다.
	// 항상 1등을 고르면 결정론적이라 리스폰 지점이 한 곳으로 굳어버린다.
	Scored.Sort([](const TPair<float, APlayerStart*>& A, const TPair<float, APlayerStart*>& B)
	{
		return A.Key > B.Key;
	});

	const int32 PoolSize = FMath::Clamp(SpawnCandidatePoolSize, 1, Scored.Num());
	return Scored[FMath::RandRange(0, PoolSize - 1)].Value;
}

bool AGameGameMode::IsStartOccupied(const APlayerStart* Start) const
{
	UWorld* World = GetWorld();
	if (!World || !Start)
	{
		return true;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnClearance), false);
	Params.AddIgnoredActor(Start);

	return World->OverlapAnyTestByChannel(
		Start->GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(SpawnClearanceRadius, SpawnClearanceHalfHeight),
		Params);
}

float AGameGameMode::DistanceToNearestEnemy(const APlayerStart* Start, int32 Team) const
{
	if (!GameState || !Start)
	{
		return TNumericLimits<float>::Max();
	}

	// 팀이 없는 플레이어(관전 등)에게는 모든 지점이 똑같으므로 거리 계산을 건너뛴다.
	if (!Teams::IsValidId(Team))
	{
		return TNumericLimits<float>::Max();
	}

	const FVector StartLocation = Start->GetActorLocation();
	float NearestSquared = TNumericLimits<float>::Max();

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AGamePlayerState* GamePlayerState = Cast<AGamePlayerState>(PlayerState);
		if (!GamePlayerState || GamePlayerState->GetTeam() == Team)
		{
			continue;
		}

		const APawn* EnemyPawn = GamePlayerState->GetPawn();
		if (!EnemyPawn)
		{
			continue;
		}

		NearestSquared = FMath::Min(
			NearestSquared,
			static_cast<float>(FVector::DistSquared(StartLocation, EnemyPawn->GetActorLocation())));
	}

	return NearestSquared == TNumericLimits<float>::Max()
		? NearestSquared
		: FMath::Sqrt(NearestSquared);
}
