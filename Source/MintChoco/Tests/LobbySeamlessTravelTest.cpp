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

	// 최소 인원은 인자다. 배포 빌드가 쓰는 2와 그 밖이 쓰는 1을 여기서 둘 다 검사한다 —
	// 함수 안에서 UE_BUILD_SHIPPING으로 갈랐다면 배포 쪽은 영원히 검사되지 않는다.
	constexpr int32 Solo = 1;
	constexpr int32 Shipping = 2;

	TestFalse(TEXT("nobody: not ready"), ALobbyGameMode::AreAllReady({}, Solo));
	TestFalse(TEXT("only a stale game state: not ready"), ALobbyGameMode::AreAllReady({Stale}, Solo));

	A->Ready = true;
	B->Ready = false;
	TestFalse(TEXT("one of two: not ready"), ALobbyGameMode::AreAllReady({A, B}, Solo));

	// 혼자 준비한 경우가 이번 규칙의 핵심이다. 개발 빌드는 시작되고 배포 빌드는 시작되지 않는다.
	TestTrue(TEXT("alone starts when one is enough"), ALobbyGameMode::AreAllReady({A}, Solo));
	TestFalse(TEXT("alone does not start when two are required"), ALobbyGameMode::AreAllReady({A}, Shipping));

	B->Ready = true;
	TestTrue(TEXT("both: ready"), ALobbyGameMode::AreAllReady({A, B}, Solo));
	TestTrue(TEXT("two is enough when two are required"), ALobbyGameMode::AreAllReady({A, B}, Shipping));
	TestTrue(TEXT("stale game state is ignored"), ALobbyGameMode::AreAllReady({A, Stale, B}, Solo));
	TestTrue(TEXT("spectator is ignored"), ALobbyGameMode::AreAllReady({A, B, Watcher}, Solo));

	// 머릿수를 채우는 데도 끼지 않는다. 관전자를 데려와 둘을 만들 수는 없다.
	TestFalse(TEXT("a spectator does not make up the numbers"), ALobbyGameMode::AreAllReady({A, Watcher}, Shipping));
	TestFalse(TEXT("a stale game state does not make up the numbers"), ALobbyGameMode::AreAllReady({A, Stale}, Shipping));

	// 0이나 음수를 넘겨도 아무도 없는 로비가 시작되지는 않는다.
	TestFalse(TEXT("an empty lobby never starts"), ALobbyGameMode::AreAllReady({}, 0));

	MintChocoTest::DestroyWorld(World);
	return true;
}

#endif
