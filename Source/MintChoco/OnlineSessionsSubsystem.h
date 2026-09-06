// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "OnlineSessionSettings.h"
#include "OnlineSessionsSubsystem.generated.h"

/**
 * DestroySession이 끝난 뒤에 이어서 할 일.
 *
 * 같은 이름의 세션이 남아 있으면 CreateSession은 실패하고 JoinSession은
 * AlreadyInSession을 돌려준다. 그래서 먼저 비워야 하는데 파괴가 비동기라,
 * 원래 요청을 기억해뒀다가 완료 콜백에서 이어서 실행한다.
 */
enum class EPendingSessionAction : uint8
{
	None,
	Create,
	Join,
};

/**
 *
 */

USTRUCT(BlueprintType)
struct FMySessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString RoomName;

	UPROPERTY(BlueprintReadWrite)
	FString HostName;

	UPROPERTY(BlueprintReadWrite)
	int32 MaxPlayer;

	UPROPERTY(BlueprintReadWrite)
	int32 JoinPlayerCount;

	UPROPERTY(BlueprintReadWrite)
	int32 PingSpeed;

	UPROPERTY(BlueprintReadWrite)
	int32 Index;

	void Print()
	{
		FString log = FString::Printf(TEXT("[%d]%s : %s, %d/%d %dms"),
			Index,
			*RoomName,
			*HostName,
			JoinPlayerCount,
			MaxPlayer,
			PingSpeed);

		UE_LOG(LogTemp, Warning, TEXT("%s"), *log);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSearchSignature, const struct FMySessionInfo&, SessionInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSearchLockSignature, bool, bSearching);

UCLASS(BlueprintType)
class MINTCHOCO_API UOnlineSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
// public:
// 	UFUNCTION(BlueprintCallable)
// 	void CreateSession();
//
// private:
// 	void OnCreate(FName SessionName, bool bWasSuccessful);

private:
	IOnlineSessionPtr SessionManager;

	FOnCreateSessionCompleteDelegate CreateCompleteDelegate;
	FDelegateHandle CreateCompleteDelegateHandle;

public:
	IOnlineSessionPtr SessionInterface;
	// NULL 서브시스템(= LAN)으로 동작 중인지. Initialize에서 월드별 인스턴스를 보고 캐시한다.
	bool bIsLanSubsystem = false;

	FSearchSignature OnSearchComplete;
	FSearchLockSignature OnSearchLockComplete;

	FDelegateHandle CreateSessionDelegateHandle;
	FDelegateHandle FindSessionDelegateHandle;
	FDelegateHandle JoinSessionDelegateHandle;
	FDelegateHandle DestroySessionDelegateHandle;
	FDelegateHandle UserInviteDelegateHandle;

	// 광고할 호스트 이름(플랫폼 닉네임). 못 얻으면 "Unknown".
	// UFUNCTION(BlueprintCallable)
	// FString GetLocalPlayerNickname() const;

	// 방생성 요청
	void OnMyCreateSession(FString roomName, int32 maxPlayer);
	// 방생성 응답
	void OnMyCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	// 방검색
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	void OnMyFindSessions();
	void OnMyFindSessionsComplete(bool bWasSuccessful);

	// 방참여 요청
	void OnMyJoinSession(int32 index);
	// 방생성 응답
	void OnMyJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	// 방퇴장 요청
	void OnMyExitRoom();
	// 방퇴장 응답
	void OnMyDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	void OnMyInviteAcceptedComplete(bool bWasSuccessful, int ControllerId, TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult);

	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type ErrorType, const FString& String);

private:
	/** NAME_GameSession이 이미 살아 있는지. 생성·참가 전에 반드시 확인한다. */
	bool HasExistingSession() const;

	/** 검색 결과 하나로 참가를 시작한다. 목록 클릭과 초대 수락이 함께 쓴다. */
	void JoinSessionResult(const FOnlineSessionSearchResult& SearchResult);

	/** 기존 세션 정리가 끝난 뒤 실제 요청을 보내는 지점. */
	void CreateSessionInternal();
	void JoinSessionInternal();

	void TravelToRoomList();

	EPendingSessionAction PendingAction = EPendingSessionAction::None;

	FString PendingRoomName;
	int32 PendingMaxPlayer = 0;

	/**
	 * 참가할 세션. 인덱스가 아니라 결과를 통째로 들고 있는다. 파괴를 기다리는 사이에
	 * 새 검색이 SessionSearch를 갈아치우면 인덱스는 다른 방을 가리키게 된다.
	 */
	FOnlineSessionSearchResult PendingJoinResult;

	FText LocalPlayerNickname;

	void SetLocalPlayerNickname();
public:
	UFUNCTION(BlueprintCallable)
	FText GetLocalPlayerNicknameToFText() const{ return LocalPlayerNickname;};
	FString GetLocalPlayerNicknameToFString() const{ return LocalPlayerNickname.ToString();};

private:
	FString StringBase64Encoder(const FString& str);
	FString StringBase64Decoder(const FString& str);
};


