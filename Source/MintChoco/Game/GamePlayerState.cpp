// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/GamePlayerState.h"

#include "Engine/World.h"
#include "Game/GameGameState.h"
#include "Game/Unit.h"
#include "GameFramework/PlayerController.h"
#include "MintChoco.h"
#include "Net/UnrealNetwork.h"
#include "Screen/ScreenFadeSubsystem.h"
#include "TimerManager.h"

void AGamePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGamePlayerState, Nickname);
	DOREPLIFETIME(AGamePlayerState, Team);
	DOREPLIFETIME(AGamePlayerState, bReady);
}

void AGamePlayerState::BeginPlay()
{
	Super::BeginPlay();

	// 게임 맵에서만 의미가 있다. 로비·타이틀의 GameState는 AGameGameState가 아니다.
	if (GetWorld() && GetWorld()->GetGameState<AGameGameState>())
	{
		GetWorld()->GetTimerManager().SetTimer(ReadyPollTimer, this, &AGamePlayerState::PollLocalReady, 0.2f, /*bLoop=*/true);
	}
}

void AGamePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ReadyPollTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AGamePlayerState::PollLocalReady()
{
	UWorld* const World = GetWorld();
	const AGameGameState* const State = World ? World->GetGameState<AGameGameState>() : nullptr;
	if (!State || State->GetMatchPhase() != EMatchPhase::WaitingForPlayers || bReady)
	{
		// 이미 시작됐거나 보고를 마쳤다.
		World->GetTimerManager().ClearTimer(ReadyPollTimer);
		return;
	}

	// 컨트롤러는 소유 머신에만 존재한다. 다른 플레이어의 PlayerState는 여기서 끝난다.
	// 단 클라이언트에서는 내 컨트롤러 참조가 조금 늦게 풀릴 수 있어, 없을 때는 계속 본다.
	const APlayerController* const PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		if (HasAuthority())
		{
			// 서버에는 모든 컨트롤러가 있다. 없다는 것은 이 PlayerState가 컨트롤러 없이 남은 것이다.
			World->GetTimerManager().ClearTimer(ReadyPollTimer);
		}
		return;
	}
	if (!PlayerController->IsLocalController())
	{
		World->GetTimerManager().ClearTimer(ReadyPollTimer);
		return;
	}

	const UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(this);
	if (!IsValid(PlayerController->GetPawn()) || (Fade && Fade->IsCovered()))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(ReadyPollTimer);
	if (HasAuthority())
	{
		bReady = true;
	}
	else
	{
		ServerSetReady();
	}
	UE_LOG(LogMintChoco, Log, TEXT("%s: 준비 완료."), *GetPlayerName());
}

void AGamePlayerState::ServerSetReady_Implementation()
{
	bReady = true;
}

void AGamePlayerState::OnRep_Team()
{
	// 폰이 아직 붙지 않았으면 뒤이어 AUnit::OnRep_PlayerState가 같은 일을 한다.
	// 둘 중 무엇이 먼저 도착하든 한 번은 반영되도록 양쪽에 걸어 둔다.
	if (AUnit* Unit = Cast<AUnit>(GetPawn()))
	{
		Unit->ApplyTeamToWeapon();
	}
}
