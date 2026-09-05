// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/GameGameMode.h"

#include "EngineUtils.h"
#include "Game/GamePlayerState.h"
#include "Game/TeamPlayerStart.h"
#include "Game/Unit.h"
#include "Game/UnitDataAsset.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "MintChoco.h"

AGameGameMode::AGameGameMode()
{
	PlayerStateClass = AGamePlayerState::StaticClass();
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
