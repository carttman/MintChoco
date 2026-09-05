// Fill out your copyright notice in the Description page of Project Settings.


#include "OnlineSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include <string>

/** Steam appid 480은 전 세계가 공유하는 로비 풀이라, 우리 세션만 식별할 키가 필요하다. */
static const FName SETTING_GAME_ID = FName("MINTCHOCO_ID");
static const FString MINTCHOCO_GAME_ID = TEXT("MintChoco_v1");

/**
 * 진단 스위치. true면 세션 필터를 하나도 걸지 않고 검색된 로비를 전부 로그로 남긴다.
 * 무필터 검색은 appid 480 전역 풀에서 임의의 일부만 돌려주므로 우리 방이 밀려날 수 있다.
 * 문제를 재현할 때만 잠시 true로 둘 것.
 */
static const bool bDiagnoseSessionFilters = false;



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

	// PIE에서는 월드마다 별도의 OSS 인스턴스가 존재한다. 월드 없이 IOnlineSubsystem::Get()을
	// 부르면 모든 PIE 인스턴스가 전역 인스턴스 하나를 공유하게 됨.
	if (auto* subsys = Online::GetSubsystem(GetWorld()))
	{
		bIsLanSubsystem = FName("NULL") == subsys->GetSubsystemName();

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

	SetLocalPlayerNickname();
}

void UOnlineSessionsSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.Remove(CreateSessionDelegateHandle);
		SessionInterface->OnFindSessionsCompleteDelegates.Remove(FindSessionDelegateHandle);
		SessionInterface->OnJoinSessionCompleteDelegates.Remove(JoinSessionDelegateHandle);
		SessionInterface->OnDestroySessionCompleteDelegates.Remove(DestroySessionDelegateHandle);
		SessionInterface->OnSessionUserInviteAcceptedDelegates.Remove(UserInviteDelegateHandle);
	}
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
	settings.bIsLANMatch = bIsLanSubsystem;
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
	// 호스트 이름은 플랫폼 닉네임을 쓴다. 방 이름과 같은 값이 들어가면 목록에서 구분이 안 된다.
	settings.Set(FName("HOST_NAME"), StringBase64Encoder(GetLocalPlayerNicknameToFString()),
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// Steam은 이 키를 로비 문자열 필터로 번역하므로, 남의 480 로비는 결과에 내려오지 않는다.
	settings.Set(SETTING_GAME_ID, MINTCHOCO_GAME_ID,
				 EOnlineDataAdvertisementType::ViaOnlineService);

	FUniqueNetIdPtr netID = GetWorld()->GetFirstLocalPlayerFromController()->GetUniqueNetIdForPlatformUser().
										GetUniqueNetId();

	UE_LOG(LogTemp, Warning, TEXT("OnMyCreateSession : %s, BuildId : 0x%08x"), *roomName, GetBuildUniqueId());

	SessionInterface->CreateSession(*netID, NAME_GameSession, settings);
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
	if (false == bDiagnoseSessionFilters)
	{
		SessionSearch->QuerySettings.Set(SETTING_GAME_ID, MINTCHOCO_GAME_ID, EOnlineComparisonOp::Equals);
	}
	SessionSearch->bIsLanQuery = bIsLanSubsystem;
	SessionSearch->MaxSearchResults = 50;

	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());

	OnSearchLockComplete.Broadcast(true);
}

void UOnlineSessionsSubsystem::OnMyFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnMyFindSessionsComplete : bWasSuccessful : %d, BuildId : 0x%08x"),
		   bWasSuccessful, GetBuildUniqueId());

	OnSearchLockComplete.Broadcast(false);
	if (bWasSuccessful)
	{
		TArray<FOnlineSessionSearchResult> results = SessionSearch->SearchResults;

		// 0이면 Steam이 아무것도 돌려주지 않은 것, 그 이상이면 아래 필터가 범인인지 알 수 있다.
		UE_LOG(LogTemp, Warning, TEXT("SearchResults : %d"), results.Num());

		for (int32 i = 0; i < results.Num(); i++)
		{
			FOnlineSessionSearchResult& ssr = results[i];
			if (false == ssr.IsValid()) continue;

			FString gameId;
			const bool bHasGameId = ssr.Session.SessionSettings.Get(SETTING_GAME_ID, gameId);

			if (bDiagnoseSessionFilters)
			{
				UE_LOG(LogTemp, Warning,
					   TEXT("  [%d] GAME_ID '%s' (found %d), BuildId 0x%08x, Owner '%s', %d/%d"),
					   i, *gameId, bHasGameId,
					   ssr.Session.SessionSettings.BuildUniqueId,
					   *ssr.Session.OwningUserName,
					   ssr.Session.SessionSettings.NumPublicConnections - ssr.Session.NumOpenPublicConnections,
					   ssr.Session.SessionSettings.NumPublicConnections);
			}
			// NULL 서브시스템(LAN)은 QuerySettings를 무시하므로 여기서 한 번 더 거른다.
			else if (false == bHasGameId || gameId != MINTCHOCO_GAME_ID)
			{
				UE_LOG(LogTemp, Warning, TEXT("Filtered out by GAME_ID : '%s'"), *gameId);
				continue;
			}

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
	SessionInterface->JoinSession(0, NAME_GameSession, sr);
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
	SessionInterface->DestroySession(NAME_GameSession);
}

void UOnlineSessionsSubsystem::OnMyDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		auto pc = GetWorld()->GetFirstPlayerController();
		pc->ClientTravel(TEXT("/Game/Maps/Room"), TRAVEL_Absolute);
	}
}

void UOnlineSessionsSubsystem::OnMyInviteAcceptedComplete(bool bWasSuccessful, int ControllerId,
	TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful)
	{
		SessionInterface->JoinSession(0, NAME_GameSession, InviteResult);
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

void UOnlineSessionsSubsystem::SetLocalPlayerNickname()
{
	if (auto* subsys = Online::GetSubsystem(GetWorld()))
	{
		if (const IOnlineIdentityPtr identity = subsys->GetIdentityInterface())
		{
			const FString nickname = identity->GetPlayerNickname(0);
			if (false == nickname.IsEmpty())
			{
				LocalPlayerNickname = FText::FromString(nickname);
			}
		}
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
