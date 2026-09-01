// Fill out your copyright notice in the Description page of Project Settings.


#include "OnlineSessionsSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

UOnlineSessionsSubsystem::UOnlineSessionsSubsystem()
{

}

void UOnlineSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem == nullptr)
		return;

	SessionManager = OnlineSubsystem->GetSessionInterface();
	if (SessionManager.IsValid() == false)
		return;

	FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Cyan, SubsystemName);


}

void UOnlineSessionsSubsystem::CreateSession()
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 10;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUseLobbiesIfAvailable = true;

	const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	SessionManager->CreateSession(*localPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings);

}
