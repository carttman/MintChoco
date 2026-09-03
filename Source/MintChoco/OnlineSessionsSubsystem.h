// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "OnlineSessionsSubsystem.generated.h"

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

UCLASS()
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
	FString GetLocalPlayerNickname() const;

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


	FString StringBase64Encoder(const FString& str);
	FString StringBase64Decoder(const FString& str);
};


