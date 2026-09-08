// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyWidget.h"
#include "LobbyUserWidget.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameState.h"

bool ULobbyWidget::Initialize()
{
	if (Super::Initialize() == false)
		return false;

	SetInfo();

	return true;
}

void ULobbyWidget::SetInfo()
{
	// Clear Children
	UserList->ClearChildren();
	LobbyUsers.Empty();

	// Create Child Widget
	for (int32 i = 0; i < 10; i++)
	{
		if (LobbyUserWidgetClass == nullptr)
			continue;

		ULobbyUserWidget* ChildWidget = CreateWidget<ULobbyUserWidget>(GetWorld(), LobbyUserWidgetClass);
		if (ChildWidget == nullptr)
			continue;

		UserList->AddChildToVerticalBox(ChildWidget);

		LobbyUsers.Add(ChildWidget);
	}

	RefreshUI();
}

void ULobbyWidget::RefreshUI()
{
	AGameStateBase* GameState = UGameplayStatics::GetGameState(this);
	if (GameState == nullptr)
		return;

	// Cache Player Length
	TArray<ALobbyPlayerState*> LobbyPlayerStates = GetLobbyPlayerStates();
	const int32 PlayerLength = LobbyPlayerStates.Num();

	for (int32 i = 0; i < LobbyUsers.Num(); i++)
	{
		const int32 Index = i;

		if (Index < PlayerLength)
		{
			// Show UI
			LobbyUsers[Index]->SetVisibility(ESlateVisibility::Visible);

			// SetInfo
			ALobbyPlayerState* PlayerState = GetLobbyPlayerStateAtIndex(Index);
			LobbyUsers[Index]->SetInfo(PlayerState);
		}
		else
		{
			// Hide UI
			LobbyUsers[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Show GameConfig Button
	if (UKismetSystemLibrary::IsServer(this))
		Btn_GameConfig->SetVisibility(ESlateVisibility::Visible);
	else
		Btn_GameConfig->SetVisibility(ESlateVisibility::Hidden);
}

TArray<ALobbyPlayerState*> ULobbyWidget::GetLobbyPlayerStates()
{
	TArray<ALobbyPlayerState*> LobbyPlayerStates;

	if (AGameStateBase* GameState = UGameplayStatics::GetGameState(this))
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState);
			if (LobbyPlayerState == nullptr)
				continue;

			LobbyPlayerStates.Add(LobbyPlayerState);
		}
	}

	// PlayerArray의 순서는 머신마다 다르다. 클라이언트에서는 PlayerState 액터가 복제되어
	// 도착한 순서대로 담기기 때문이다. 그래서 그대로 쓰면 각자 다른 순서로 보인다.
	//
	// PlayerId는 서버가 입장 순서대로 매기고(AGameSession::RegisterPlayer) COND_InitialOnly로
	// 복제되므로, PlayerState가 존재하는 시점에는 이미 값이 들어 있다. 이걸로 정렬하면
	// 모든 화면이 호스트와 같은 순서가 된다.
	//
	// TArray<T*>::Sort는 포인터를 역참조해서 프레디케이트에 넘긴다. 위에서 null을 걸렀으므로 안전하다.
	LobbyPlayerStates.Sort([](const ALobbyPlayerState& A, const ALobbyPlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});

	return LobbyPlayerStates;
}

ALobbyPlayerState* ULobbyWidget::GetLobbyPlayerStateAtIndex(int32 InIndex)
{
	TArray<ALobbyPlayerState*> LobbyPlayerStates = GetLobbyPlayerStates();

	if (InIndex < LobbyPlayerStates.Num())
		return LobbyPlayerStates[InIndex];

	return nullptr;
}
