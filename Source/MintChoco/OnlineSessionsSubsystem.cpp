// Fill out your copyright notice in the Description page of Project Settings.


#include "OnlineSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include <string>



void UOnlineSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	// if (OnlineSubsystem == nullptr)
	// 	return;
	//
	// SessionManager = OnlineSubsystem->GetSessionInterface();
	// if (SessionManager.IsValid() == false)
	// 	return;
	//
	// FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
	// if (GEngine)
	// 	GEngine->AddOnScreenDebugMessage(-1, 5, FColor::Cyan, SubsystemName);
	//
	// //SessionManager->OnCreateSessionCompleteDelegates.AddUObject(this, &UOnlineSessionsSubsystem::OnCreate);

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UOnlineSessionsSubsystem::OnNetworkFailure);
	}

	if (auto* subsys = IOnlineSubsystem::Get())
	{
		SessionInterface = subsys->GetSessionInterface();
		if (SessionInterface)
		{
			CreateSessionDelegateHandle = SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
				this, &UOnlineSessionsSubsystem::OnMyCreateSessionComplete);

			FindSessionDelegateHandle = SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
				this, &UOnlineSessionsSubsystem::OnMyFindSessionsComplete);

			JoinSessionDelegateHandle = SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
				this, &UOnlineSessionsSubsystem::OnMyJoinSessionComplete);

			DestroySessionDelegateHandle = SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
				this, &UOnlineSessionsSubsystem::OnMyDestroySessionComplete);

			UserInviteDelegateHandle = SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(
				this, &UOnlineSessionsSubsystem::OnMyInviteAcceptedComplete);
		}
	}
}

void UOnlineSessionsSubsystem::Deinitialize()
{
	SessionInterface->OnCreateSessionCompleteDelegates.Remove(CreateSessionDelegateHandle);
	SessionInterface->OnFindSessionsCompleteDelegates.Remove(FindSessionDelegateHandle);
	SessionInterface->OnJoinSessionCompleteDelegates.Remove(JoinSessionDelegateHandle);
	SessionInterface->OnDestroySessionCompleteDelegates.Remove(DestroySessionDelegateHandle);
	SessionInterface->OnSessionUserInviteAcceptedDelegates.Remove(UserInviteDelegateHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	Super::Deinitialize();
}

void UOnlineSessionsSubsystem::OnMyCreateSession(FString roomName, int32 maxPlayer)
{
	FOnlineSessionSettings settings;

	settings.bIsDedicated = false;
	// true 랜매치인가? false 스팀인가?
	settings.bIsLANMatch = FName("NULL") == IOnlineSubsystem::Get()->GetSubsystemName();
	// 매칭이 온라인에 노출시킬것인가?
	settings.bShouldAdvertise = true;
	// 온라인 상태 정보를 활용할것인가?
	settings.bUsesPresence = true;
	// 로비를 사용할것인가?
	settings.bUseLobbiesIfAvailable = true;
	// 게임진행중에 참가여부
	settings.bAllowJoinInProgress = true;
	settings.bAllowJoinViaPresence = true;

	settings.NumPublicConnections = maxPlayer;

	// 커스텀 설정
	settings.Set(FName("ROOM_NAME"), StringBase64Encoder(roomName),
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	settings.Set(FName("HOST_NAME"), StringBase64Encoder(MySessionName),
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	FUniqueNetIdPtr netID = GetWorld()->GetFirstLocalPlayerFromController()->GetUniqueNetIdForPlatformUser().
										GetUniqueNetId();

	UE_LOG(LogTemp, Warning, TEXT("OnMyCreateSession : %s"), *MySessionName);

	SessionInterface->CreateSession(*netID, FName(MySessionName), settings);
}

void UOnlineSessionsSubsystem::OnMyCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnMyCreateSessionComplete : SessionName : %s, bWasSuccessful : %d"),
		   *SessionName.ToString(), bWasSuccessful);
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSession Success!!!"));
		GetWorld()->ServerTravel(TEXT("/Game/Maps/Lobby?listen"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSession Failed..."));
	}
}

void UOnlineSessionsSubsystem::OnMyFindSessions()
{
	SessionSearch = MakeShareable(new FOnlineSessionSearch());

	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->bIsLanQuery = FName("NULL") == IOnlineSubsystem::Get()->GetSubsystemName();
	SessionSearch->MaxSearchResults = 50;

	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());

	OnSearchLockComplete.Broadcast(true);
}

void UOnlineSessionsSubsystem::OnMyFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnMyFindSessionsComplete"));

	OnSearchLockComplete.Broadcast(false);
	if (bWasSuccessful)
	{
		TArray<FOnlineSessionSearchResult> results = SessionSearch->SearchResults;

		for (int32 i = 0; i < results.Num(); i++)
		{
			FOnlineSessionSearchResult& ssr = results[i];
			if (false == ssr.IsValid()) continue;

			FMySessionInfo sessionInfo;

			sessionInfo.Index = i;

			ssr.Session.SessionSettings.Get(FName("ROOM_NAME"), OUT sessionInfo.RoomName);
			ssr.Session.SessionSettings.Get(FName("HOST_NAME"), OUT sessionInfo.HostName);

			sessionInfo.RoomName = StringBase64Decoder(sessionInfo.RoomName);
			sessionInfo.HostName = StringBase64Decoder(sessionInfo.HostName);

			sessionInfo.MaxPlayer = ssr.Session.SessionSettings.NumPublicConnections;
			// 현재 입장 수 = 총수 - 입장가능수
			sessionInfo.JoinPlayerCount = sessionInfo.MaxPlayer - ssr.Session.NumOpenPublicConnections;
			sessionInfo.PingSpeed = ssr.PingInMs;

			sessionInfo.Print();

			if (OnSearchComplete.IsBound())
			{
				OnSearchComplete.Broadcast(sessionInfo);
			}
		}
	}
}

void UOnlineSessionsSubsystem::OnMyJoinSession(int32 index)
{
	auto sr = SessionSearch->SearchResults[index];
	SessionInterface->JoinSession(0, FName(MySessionName), sr);
}

void UOnlineSessionsSubsystem::OnMyJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (false == SessionInterface.IsValid())
		return;
	if (Result != EOnJoinSessionCompleteResult::Type::Success)
		return;

	FString url;
	SessionInterface->GetResolvedConnectString(SessionName, url);
	UE_LOG(LogTemp, Warning, TEXT("join url : %s"), *url);
	auto* pc = GetWorld()->GetFirstPlayerController();
	if (false == url.IsEmpty() && pc)
	{
		pc->ClientTravel(url, TRAVEL_Absolute);
	}
}

void UOnlineSessionsSubsystem::OnMyExitRoom()
{
	SessionInterface->DestroySession(FName(MySessionName));
}

void UOnlineSessionsSubsystem::OnMyDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		auto pc = GetWorld()->GetFirstPlayerController();
		pc->ClientTravel(TEXT("/Game/MutiPlayer/Maps/LobbyMap"), TRAVEL_Absolute);
	}
}

void UOnlineSessionsSubsystem::OnMyInviteAcceptedComplete(bool bWasSuccessful, int ControllerId,
	TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful)
	{
		FString roomName;
		InviteResult.Session.SessionSettings.Get(FName("ROOM_NAME"), roomName);
		roomName = StringBase64Decoder(roomName);
		SessionInterface->JoinSession(0, FName(roomName), InviteResult);
	}
}

void UOnlineSessionsSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type ErrorType,
	const FString& String)
{
	switch (ErrorType)
	{
	case ENetworkFailure::Type::ConnectionLost:
		OnMyExitRoom();
		break;
	}
}

FString UOnlineSessionsSubsystem::StringBase64Encoder(const FString& str)
{
	std::string utf8string = TCHAR_TO_UTF8(*str);
	TArray<uint8> bytes = TArray<uint8>((uint8*)utf8string.c_str(), utf8string.length());
	return FBase64::Encode(bytes);
}

FString UOnlineSessionsSubsystem::StringBase64Decoder(const FString& str)
{
	TArray<uint8> bytes;
	FBase64::Decode(str, bytes);
	std::string utf8string((char*)bytes.GetData(), bytes.Num());
	return UTF8_TO_TCHAR(utf8string.c_str());
}
