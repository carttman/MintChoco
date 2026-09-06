#include "Ink/InkTankComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include "MintChoco.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GSetInkCommand(
		TEXT("mc.Ink.Set"),
		TEXT("Sets the first local player's ink tank to a fraction of a full tank (0..1). Needs authority over the pawn."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 1)
			{
				UE_LOG(LogMintChoco, Warning, TEXT("Usage: mc.Ink.Set <0..1>"));
				return;
			}

			const APlayerController* const Controller = World->GetFirstPlayerController();
			const APawn* const Pawn = Controller ? Controller->GetPawn() : nullptr;
			UInkTankComponent* const Tank = Pawn ? Pawn->FindComponentByClass<UInkTankComponent>() : nullptr;
			if (!Tank)
			{
				UE_LOG(LogMintChoco, Warning, TEXT("mc.Ink.Set: the local pawn has no ink tank."));
				return;
			}
			if (!Pawn->HasAuthority())
			{
				UE_LOG(LogMintChoco, Warning, TEXT("mc.Ink.Set: no authority over the pawn here; run it on the server."));
				return;
			}

			Tank->SetInk(FCString::Atof(*Args[0]));
		}));
}

UInkTankComponent::UInkTankComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UInkTankComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInkTankComponent, Ink);
}

void UInkTankComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only the server refills. A client that refilled on its own would drift upward whenever the
	// server's value sits still, and nothing would ever pull it back down.
	if (GetOwnerRole() == ROLE_Authority)
	{
		Refill(DeltaTime);
	}
}

void UInkTankComponent::Refill(float Seconds)
{
	if (RefillPause > 0.0f)
	{
		const float Paused = FMath::Min(RefillPause, Seconds);
		RefillPause -= Paused;
		Seconds -= Paused;
	}

	if (Seconds > 0.0f && Ink < 1.0f)
	{
		ApplyInk(Ink + RefillPerSecond * Seconds);
	}
}

bool UInkTankComponent::TryConsume(float Cost)
{
	if (!CanAfford(Cost))
	{
		return false;
	}

	RefillPause = RefillDelayAfterSpend;
	ApplyInk(Ink - Cost);
	return true;
}

void UInkTankComponent::SetInk(float NewInk)
{
	ApplyInk(NewInk);
}

void UInkTankComponent::OnRep_Ink()
{
	OnInkChanged.Broadcast(Ink);
}

void UInkTankComponent::ApplyInk(float NewInk)
{
	NewInk = FMath::Clamp(NewInk, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(NewInk, Ink))
	{
		return;
	}

	Ink = NewInk;
	OnInkChanged.Broadcast(Ink);
}
