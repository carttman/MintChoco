// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

/**
 * 로비 게임모드.
 *
 * 플레이어는 두 경로로 들어온다. 방에서 처음 들어올 때는 PostLogin, 경기를 마치고
 * 돌아올 때는 Seamless Travel의 HandleSeamlessTravelPlayer다. 두 번째 경로에서는
 * 엔진이 PlayerController와 PlayerState를 새로 만들고 PostLogin을 부르지 않으므로,
 * 플레이어별 초기화는 반드시 InitLobbyPlayer 한 곳에 두고 양쪽에서 부른다.
 */
UCLASS()
class MINTCHOCO_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * PlayerState가 처음 복제되기 전에 닉네임을 채워 넣는다.
	 *
	 * 클라이언트가 RPC로 알려주기를 기다리면 왕복이 한 번 더 들어가, 이름이 빈 칸으로
	 * 먼저 보였다가 뒤늦게 채워진다.
	 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/**
	 * 경기에서 Seamless Travel로 돌아온 플레이어. Super가 새 컨트롤러·PlayerState·폰을
	 * 만든 뒤에 PostLogin과 같은 초기화를 한다.
	 */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_TryStartGame();

	UFUNCTION(BlueprintCallable)
	void TryStartGame();

	/**
	 * 로비의 모든 플레이어가 준비했는지. 블루프린트 IsEveryBodyReady는 PlayerArray의 모든
	 * 항목을 BP_LobbyPlayerState로 캐스팅하는데, Seamless Travel 직후에는 아직 안 지워진 게임
	 * PlayerState나 관전자가 섞여 있어 캐스팅이 실패하고 영원히 false가 된다. 여기서는
	 * 로비 PlayerState만 세고, 비활성·관전자는 건너뛴다. 아무도 없으면 false.
	 *
	 * 센 사람이 MinPlayersToStart보다 적으면 전원이 준비해도 거짓이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool AreAllPlayersReady() const;

	/**
	 * 게임을 시작하는 데 필요한 최소 인원. 배포 빌드는 둘 이상이어야 하고, 그 밖의 빌드는
	 * 혼자서도 시작된다 — 맵과 아이템은 혼자 확인하는 일이 대부분이기 때문이다.
	 *
	 * 에디터에서 만질 수 있는 UPROPERTY로 두지 않는다. 빌드에 따라 기본값이 달라지는 프로퍼티는
	 * 누군가 한 번 건드리는 순간 그 값이 BP_LobbyGameMode의 CDO에 직렬화되고, 개발 빌드에서
	 * 정해진 1이 배포 빌드까지 따라가 이 규칙을 경고 한 줄 없이 없앤다.
	 */
	static constexpr int32 MinPlayersToStart = UE_BUILD_SHIPPING ? 2 : 1;

	/**
	 * AreAllPlayersReady의 순수 판정. 테스트용.
	 *
	 * 최소 인원을 상수로 읽지 않고 인자로 받는 이유가 있다. 안에서 UE_BUILD_SHIPPING으로 갈라
	 * 버리면 배포 쪽 분기는 영원히 테스트되지 않는다 — 테스트는 배포 빌드에서 돌지 않는다.
	 */
	static bool AreAllReady(const TArray<const APlayerState*>& PlayerStates, int32 MinPlayers);

protected:
	/**
	 * 플레이어가 로비에 들어올 때마다 한 번. 처음 입장(OnPostLogin)과 경기 후 복귀
	 * 모두에서 불린다. OnPostLogin에 둔 플레이어별 로직은 복귀 때 돌지 않으므로 여기로 옮긴다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby", meta = (DisplayName = "On Player Joined Lobby"))
	void BP_OnPlayerJoinedLobby(APlayerController* Player, bool bReturningFromMatch);

private:
	void InitLobbyPlayer(APlayerController* Player, bool bReturningFromMatch);
};
