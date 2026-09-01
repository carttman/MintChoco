// Fill out your copyright notice in the Description page of Project Settings.


#include "OnlineSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"

UOnlineSessionsSubsystem::UOnlineSessionsSubsystem()
{

}

void UOnlineSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem == nullptr)
		return;

	SessionManager = OnlineSubsystem->GetSessionInterface();
	if (SessionManager.IsValid() == false)
		return;

	FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5, FColor::Cyan, SubsystemName);

	//SessionManager->OnCreateSessionCompleteDelegates.AddUObject(this, &UOnlineSessionsSubsystem::OnCreate);
}
