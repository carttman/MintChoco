// Fill out your copyright notice in the Description page of Project Settings.


#include "OnlineSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Screen/ScreenFadeSubsystem.h"
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


bool UOnlineSessionsSubsystem::HasExistingSession() const
{
	return SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

void UOnlineSessionsSubsystem::TravelToRoomList()
{
	// 페이드 아웃 뒤에 떠난다. 가림막 서브시스템이 없을 때만 바로 간다.
	if (UScreenFadeSubsystem* const Fade = GetGameInstance() ? GetGameInstance()->GetSubsystem<UScreenFadeSubsystem>() : nullptr)
	{
		Fade->ClientTravelWithFade(TEXT("/Game/Maps/Room"));
		return;
	}

	UWorld* world = GetWorld();
	APlayerController* pc = world ? world->GetFirstPlayerController() : nullptr;
	if (pc)
	{
		pc->ClientTravel(TEXT("/Game/Maps/Room"), TRAVEL_Absolute);
	}
}

void UOnlineSessionsSubsystem::OnMyCreateSession(FString roomName, int32 maxPlayer)
{
	if (false == SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyCreateSession : 세션 인터페이스가 없습니다. 온라인 서브시스템 설정을 확인하세요."));
		return;
	}

	PendingRoomName = roomName;
	PendingMaxPlayer = maxPlayer;

	// 같은 이름의 세션이 남아 있으면 CreateSession은 그대로 실패한다. 게임에서 방으로
	// 돌아왔거나 트래블이 실패했을 때 흔히 남아 있으므로 먼저 비운다.
	if (HasExistingSession())
	{
		PendingAction = EPendingSessionAction::Create;

		if (false == SessionInterface->DestroySession(NAME_GameSession))
		{
			UE_LOG(LogTemp, Error, TEXT("OnMyCreateSession : 기존 세션 파괴 요청이 거부되었습니다."));
			PendingAction = EPendingSessionAction::None;
		}
		return;
	}

	CreateSessionInternal();
}

void UOnlineSessionsSubsystem::CreateSessionInternal()
{
	ULocalPlayer* localPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (nullptr == localPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSessionInternal : 로컬 플레이어가 없습니다."));
		return;
	}

	const FUniqueNetIdPtr netID = localPlayer->GetUniqueNetIdForPlatformUser().GetUniqueNetId();
	if (false == netID.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSessionInternal : NetId가 무효합니다. 스팀 로그인 상태를 확인하세요."));
		return;
	}

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

	settings.NumPublicConnections = PendingMaxPlayer;

	// 커스텀 설정
	settings.Set(FName("ROOM_NAME"), StringBase64Encoder(PendingRoomName),
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// 호스트 이름은 플랫폼 닉네임을 쓴다. 방 이름과 같은 값이 들어가면 목록에서 구분이 안 된다.
	settings.Set(FName("HOST_NAME"), StringBase64Encoder(GetLocalPlayerNicknameToFString()),
				 EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// Steam은 이 키를 로비 문자열 필터로 번역하므로, 남의 480 로비는 결과에 내려오지 않는다.
	settings.Set(SETTING_GAME_ID, MINTCHOCO_GAME_ID,
				 EOnlineDataAdvertisementType::ViaOnlineService);

	UE_LOG(LogTemp, Warning, TEXT("CreateSessionInternal : %s, BuildId : 0x%08x"), *PendingRoomName, GetBuildUniqueId());

	// 반환값이 false면 완료 델리게이트가 오지 않는다. 조용히 넘어가면 UI가 응답을 영영 기다린다.
	if (false == SessionInterface->CreateSession(*netID, NAME_GameSession, settings))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateSessionInternal : CreateSession 요청이 거부되었습니다."));
	}
}

void UOnlineSessionsSubsystem::OnMyCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnMyCreateSessionComplete : SessionName : %s, bWasSuccessful : %d"),
		   *SessionName.ToString(), bWasSuccessful);
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSession Success!!!"));
		if (UScreenFadeSubsystem* const Fade = GetGameInstance() ? GetGameInstance()->GetSubsystem<UScreenFadeSubsystem>() : nullptr)
		{
			Fade->ServerTravelWithFade(TEXT("/Game/Maps/Lobby?listen"));
		}
		else
		{
			GetWorld()->ServerTravel(TEXT("/Game/Maps/Lobby?listen"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSession Failed..."));
	}
}

void UOnlineSessionsSubsystem::OnMyFindSessions()
{
	if (false == SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyFindSessions : 세션 인터페이스가 없습니다."));
		OnSearchLockComplete.Broadcast(false);
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());

	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	if (false == bDiagnoseSessionFilters)
	{
		SessionSearch->QuerySettings.Set(SETTING_GAME_ID, MINTCHOCO_GAME_ID, EOnlineComparisonOp::Equals);
	}
	SessionSearch->bIsLanQuery = bIsLanSubsystem;
	SessionSearch->MaxSearchResults = 50;

	// 잠금을 먼저 건다. FindSessions가 완료 델리게이트를 동기적으로 부르는 경우,
	// 뒤에서 잠그면 해제가 먼저 지나가 버려 새로고침 버튼이 영영 비활성으로 남는다.
	OnSearchLockComplete.Broadcast(true);

	if (false == SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyFindSessions : FindSessions 요청이 거부되었습니다."));

		// 요청이 시작되지 않았으므로 완료 델리게이트도 오지 않는다. 직접 풀어준다.
		OnSearchLockComplete.Broadcast(false);
	}
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
	if (false == SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyJoinSession : 세션 인터페이스가 없습니다."));
		return;
	}

	// 검색을 한 번도 하지 않았거나(초대 수락 등), 목록을 새로 고치는 사이에 예전 항목을
	// 눌렀으면 인덱스가 어긋난다. 둘 다 그대로 두면 크래시다.
	if (false == SessionSearch.IsValid() || false == SessionSearch->SearchResults.IsValidIndex(index))
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyJoinSession : 검색 결과 %d번이 없습니다. 목록을 새로 고쳐 주세요."), index);
		return;
	}

	JoinSessionResult(SessionSearch->SearchResults[index]);
}

void UOnlineSessionsSubsystem::JoinSessionResult(const FOnlineSessionSearchResult& SearchResult)
{
	if (false == SearchResult.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSessionResult : 검색 결과가 무효합니다."));
		return;
	}

	// 인덱스가 아니라 결과를 복사해 둔다. 파괴를 기다리는 사이 새 검색이 SessionSearch를
	// 갈아치우면 같은 인덱스가 다른 방을 가리키게 된다.
	PendingJoinResult = SearchResult;

	// 이미 세션에 들어가 있으면 JoinSession은 AlreadyInSession으로 실패한다.
	if (HasExistingSession())
	{
		PendingAction = EPendingSessionAction::Join;

		if (false == SessionInterface->DestroySession(NAME_GameSession))
		{
			UE_LOG(LogTemp, Error, TEXT("JoinSessionResult : 기존 세션 파괴 요청이 거부되었습니다."));
			PendingAction = EPendingSessionAction::None;
		}
		return;
	}

	JoinSessionInternal();
}

void UOnlineSessionsSubsystem::JoinSessionInternal()
{
	if (false == SessionInterface->JoinSession(0, NAME_GameSession, PendingJoinResult))
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSessionInternal : JoinSession 요청이 거부되었습니다."));
	}
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
	UScreenFadeSubsystem* const Fade = GetGameInstance() ? GetGameInstance()->GetSubsystem<UScreenFadeSubsystem>() : nullptr;
	if (false == url.IsEmpty() && Fade)
	{
		Fade->ClientTravelWithFade(url);
		return;
	}

	auto* pc = GetWorld()->GetFirstPlayerController();
	if (false == url.IsEmpty() && pc)
	{
		pc->ClientTravel(url, TRAVEL_Absolute);
	}
}

void UOnlineSessionsSubsystem::OnMyExitRoom()
{
	PendingAction = EPendingSessionAction::None;

	// 파괴할 세션이 없으면 완료 델리게이트도 오지 않으므로, 여기서 직접 돌아가야
	// 플레이어가 빈 화면에 갇히지 않는다.
	if (false == SessionInterface.IsValid() || false == SessionInterface->DestroySession(NAME_GameSession))
	{
		UE_LOG(LogTemp, Warning, TEXT("OnMyExitRoom : 파괴할 세션이 없어 바로 방 목록으로 돌아갑니다."));
		TravelToRoomList();
	}
}

void UOnlineSessionsSubsystem::OnMyDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	const EPendingSessionAction action = PendingAction;
	PendingAction = EPendingSessionAction::None;

	if (false == bWasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("OnMyDestroySessionComplete : 세션 파괴에 실패했습니다."));
		return;
	}

	// 생성이나 참가를 위해 비운 것이라면 방 목록으로 돌아가면 안 된다. 이어서 진행한다.
	switch (action)
	{
	case EPendingSessionAction::Create:
		CreateSessionInternal();
		return;

	case EPendingSessionAction::Join:
		JoinSessionInternal();
		return;

	default:
		break;
	}

	TravelToRoomList();
}

void UOnlineSessionsSubsystem::OnMyInviteAcceptedComplete(bool bWasSuccessful, int ControllerId,
	TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful)
	{
		// 목록 클릭과 같은 경로를 탄다. 초대 수락 시점에도 이전 세션이 남아 있을 수 있다.
		JoinSessionResult(InviteResult);
	}
}

void UOnlineSessionsSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type ErrorType,
	const FString& String)
{
	// GEngine->OnNetworkFailure()는 UEngine이 하나만 들고 있는 전역 이벤트다.
	// PIE 창마다 GameInstance 서브시스템이 하나씩 여기에 등록되므로, 이 가드가 없으면
	// 한 창의 연결 끊김이 모든 창의 세션을 파괴한다.
	if (World != GetWorld())
	{
		return;
	}

	// 호스트는 클라이언트 하나가 끊겼다고 자기 방을 없애면 안 된다.
	if (World->GetNetMode() != NM_Client)
	{
		return;
	}

	switch (ErrorType)
	{
	case ENetworkFailure::Type::ConnectionLost:
		OnMyExitRoom();
		break;

	default:
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
