// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyUserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LobbyWidget.h"
#include "Components/EditableTextBox.h"
#include "Game/TeamLook.h"

void ULobbyUserWidget::SetInfo(ALobbyPlayerState* InPlayerState)
{
	PlayerState = InPlayerState;

	RefreshUI();
}

void ULobbyUserWidget::RefreshUI()
{
	if (PlayerState == nullptr)
		return;

	// Cache Local Variables
	Editable_PlayerName->SetIsReadOnly(true);
	bool IsReady = PlayerState->Ready;
	bool IsServer = UKismetSystemLibrary::IsServer(this);
	bool IsLocalPlayer = false;
	FText TeamText;
	if (PlayerState->Team == Teams::Mint)
		TeamText = FText::FromString("Mint");
	else if (PlayerState->Team == Teams::Choco)
		TeamText = FText::FromString("Choco");
	else
		TeamText = FText::FromString("Select Team!");

	const FColor TeamColor = Teams::IsValidId(PlayerState->Team)
		? TeamLook::GetDisplayColor(PlayerState->Team, GetWorld())
		: FColor(0, 0, 0, 255);
	Txt_Team->SetColorAndOpacity(FSlateColor(TeamColor));

	Txt_Team->SetText(TeamText);

	if (APlayerController* PlayerController = PlayerState->GetPlayerController())
		IsLocalPlayer = PlayerController->IsLocalController();

	// Hide UI
	Btn_Ready->SetVisibility(ESlateVisibility::Hidden);
	Btn_KickPlayer->SetVisibility(ESlateVisibility::Hidden);
	Txt_Ready->SetVisibility(ESlateVisibility::Hidden);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Hidden);

	// 팀 버튼은 내 줄에만 보인다. 숨기기만 하고 다시 보이게 하지 않으면 두 경우에 사라진다:
	// 풀링된 줄이 이전에 남의 플레이어를 그리며 숨긴 상태를 물려받을 때, 그리고 Seamless Travel로
	// 로비에 돌아온 직후 PlayerState가 자기 컨트롤러보다 먼저 도착해 첫 갱신에서 IsLocalPlayer가
	// false로 계산될 때(컨트롤러가 오면 ALobbyPlayerState::ClientInitialize가 다시 갱신한다).
	const ESlateVisibility TeamButtonVisibility = IsLocalPlayer ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	Btn_Mint->SetVisibility(TeamButtonVisibility);
	Btn_Choco->SetVisibility(TeamButtonVisibility);

	// Show Ready Text
	if (IsReady)
		Txt_Ready->SetVisibility(ESlateVisibility::Visible);

	// Show Ready Button
	if (IsLocalPlayer && IsReady == false)
		Btn_Ready->SetVisibility(ESlateVisibility::Visible);

	// 팀을 고르기 전에는 준비할 수 없다. 팀 없이 준비된 플레이어는 게임 맵에서 팀 없는
	// 채로 스폰되므로(Team=-1 경고) 입구에서 막는다. 서버도 Server_HandleReadyButton에서
	// 같은 조건으로 거부하므로 UI를 우회해도 준비되지 않는다.
	Btn_Ready->SetIsEnabled(Teams::IsValidId(PlayerState->Team));

	// Show Kick Button
	if (IsServer && IsLocalPlayer == false)
		Btn_KickPlayer->SetVisibility(ESlateVisibility::Visible);

	// Set Nickname
	Editable_PlayerName->SetText(PlayerState->Nickname);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Visible);

	// Txt_PlayerName->SetText(PlayerState->Nickname);
	// Txt_PlayerName->SetVisibility(ESlateVisibility::Visible);
}