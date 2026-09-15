// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "GamePlayerState.generated.h"

/**
 *
 */
UCLASS()
class MINTCHOCO_API AGamePlayerState : public APlayerState
{
	GENERATED_BODY()

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

public:
	/**
	 * 로비에서 정해져 ReceiveCopyProperties로 넘어온 팀. Teams::Mint 또는 Teams::Choco.
	 * Team이 protected라 게임모드가 직접 읽을 수 없어 여기를 통한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Team")
	FText GetNickname() const { return Nickname; }

	/**
	 * 이 플레이어의 화면이 열렸고 폰이 붙었는지. 소유 머신이 확인해 서버에 알리고, 서버가
	 * 전원 준비를 보고 카운트다운을 시작한다(AGameGameMode). 리슨 호스트는 서버 자신이 확인한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsReady() const { return bReady; }

	/**
	 * 서버 전용. 로비에서 넘어온 팀과 닉네임을 넣는다. 트래블 복사와 테스트가 쓴다.
	 * 블루프린트의 ReceiveCopyProperties가 Team/Nickname에 직접 쓰는 것과 같은 일이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetTeam(int32 InTeam) { Team = InTeam; }

	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetNickname(const FText& InNickname) { Nickname = InNickname; }

	/**
	 * Seamless Travel로 다음 맵의 PlayerState에 값을 옮긴다. 이 클래스는 C++뿐이라
	 * 블루프린트 ReceiveCopyProperties가 없으므로, 여기서 옮기지 않으면 로비로 돌아온
	 * 플레이어는 팀도 이름도 없이 시작한다(리슨 호스트는 ClientInitialize도 오지 않는다).
	 */
	virtual void CopyProperties(APlayerState* PlayerState) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(Replicated)
	bool bReady = false;

	UFUNCTION(Server, Reliable)
	void ServerSetReady();

private:
	/**
	 * 소유 머신에서 주기적으로 "폰이 있고 가림막이 걷혔는지"를 본다. 맞으면 서버에 알리고
	 * 멈춘다. 다른 플레이어의 PlayerState(컨트롤러가 없다)는 첫 검사에서 바로 멈춘다.
	 */
	void PollLocalReady();

	FTimerHandle ReadyPollTimer;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	FText Nickname;

	/**
	 * 기본값은 반드시 Teams::None이어야 한다. 0으로 두면 "팀이 정해지지 않음"과
	 * "민트"가 같은 값이 되어, 로비에서 팀을 고르지 않았거나 ReceiveCopyProperties가
	 * 값을 옮기지 못한 플레이어가 조용히 민트로 스폰된다. Teams::IsValidId(0)이
	 * true라 게임모드의 경고도 걸리지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Team)
	int32 Team = Teams::None;

	/**
	 * 팀은 폰이 아니라 이 액터에 실려 오므로, 폰의 OnRep_PlayerState와 도착 순서가
	 * 보장되지 않는다. 팀 값이 나중에 도착하는 경우 여기서 무기 색을 다시 맞춘다.
	 */
	UFUNCTION()
	void OnRep_Team();
};
