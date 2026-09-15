#include "Misc/AutomationTest.h"

#include "Game/GamePlayerState.h"
#include "Game/TeamTypes.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 게임 맵에서 로비로 Seamless Travel할 때 엔진은 PlayerState를 새로 만들고
 * 옛 것의 SeamlessTravelTo로 값을 옮긴다. 게임 쪽 PlayerState는 C++라 블루프린트
 * ReceiveCopyProperties가 없으므로, CopyProperties가 팀과 닉네임을 로비로 되돌려야 한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLobbySeamlessTravelTest,
	"MintChoco.Lobby.SeamlessTravelCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLobbySeamlessTravelTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	AGamePlayerState* const GameState = World->SpawnActor<AGamePlayerState>();
	ALobbyPlayerState* const LobbyState = World->SpawnActor<ALobbyPlayerState>();
	AGamePlayerState* const NextGameState = World->SpawnActor<AGamePlayerState>();
	if (!TestNotNull(TEXT("game player state"), GameState)
		|| !TestNotNull(TEXT("lobby player state"), LobbyState)
		|| !TestNotNull(TEXT("next game player state"), NextGameState))
	{
		MintChocoTest::DestroyWorld(World);
		return false;
	}

	const FText Nickname = FText::FromString(TEXT("형재"));
	GameState->SetTeam(Teams::Choco);
	GameState->SetNickname(Nickname);
	LobbyState->Ready = true; // 새 PlayerState는 준비 해제 상태여야 한다. 복사가 건드리면 안 된다.
	LobbyState->Ready = false;

	// 게임 -> 로비.
	GameState->SeamlessTravelTo(LobbyState);
	TestEqual(TEXT("team returns to lobby"), LobbyState->Team, Teams::Choco);
	TestTrue(TEXT("nickname returns to lobby"), LobbyState->Nickname.EqualTo(Nickname));
	TestFalse(TEXT("returning player is not ready"), LobbyState->Ready);

	// 게임 -> 게임(맵 재시작 등)도 같은 값을 잃지 않는다.
	GameState->SeamlessTravelTo(NextGameState);
	TestEqual(TEXT("team survives game to game"), NextGameState->GetTeam(), Teams::Choco);
	TestTrue(TEXT("nickname survives game to game"), NextGameState->GetNickname().EqualTo(Nickname));

	// 팀을 고르지 않은 채 왔다면 None이 그대로 온다. 0으로 뭉개지면 조용히 민트가 된다.
	AGamePlayerState* const NoTeam = World->SpawnActor<AGamePlayerState>();
	ALobbyPlayerState* const NoTeamLobby = World->SpawnActor<ALobbyPlayerState>();
	NoTeam->SeamlessTravelTo(NoTeamLobby);
	TestEqual(TEXT("no team stays none"), NoTeamLobby->Team, Teams::None);

	MintChocoTest::DestroyWorld(World);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

/** 로비의 전원 준비 판정: 로비 PlayerState만 세고, 게임 PlayerState·관전자는 건너뛴다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLobbyAllReadyTest,
	"MintChoco.Lobby.AllReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLobbyAllReadyTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	ALobbyPlayerState* const A = World->SpawnActor<ALobbyPlayerState>();
	ALobbyPlayerState* const B = World->SpawnActor<ALobbyPlayerState>();
	AGamePlayerState* const Stale = World->SpawnActor<AGamePlayerState>();
	ALobbyPlayerState* const Watcher = World->SpawnActor<ALobbyPlayerState>();
	Watcher->SetIsSpectator(true);

	TestFalse(TEXT("nobody: not ready"), ALobbyGameMode::AreAllReady({}));
	TestFalse(TEXT("only a stale game state: not ready"), ALobbyGameMode::AreAllReady({Stale}));

	A->Ready = true;
	B->Ready = false;
	TestFalse(TEXT("one of two: not ready"), ALobbyGameMode::AreAllReady({A, B}));

	B->Ready = true;
	TestTrue(TEXT("both: ready"), ALobbyGameMode::AreAllReady({A, B}));
	TestTrue(TEXT("stale game state is ignored"), ALobbyGameMode::AreAllReady({A, Stale, B}));
	TestTrue(TEXT("spectator is ignored"), ALobbyGameMode::AreAllReady({A, B, Watcher}));

	MintChocoTest::DestroyWorld(World);
	return true;
}

#endif
