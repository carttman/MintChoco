// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyUserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LobbyWidget.h"
#include "Components/EditableTextBox.h"

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
	{
		TeamText = FText::FromString("Mint");

		FColor MintColor = FColor(62, 180, 137, 255);
		Txt_Team->SetColorAndOpacity(FSlateColor(MintColor));
	}
	else if (PlayerState->Team == Teams::Choco)
	{
		TeamText = FText::FromString("Choco");

		FColor ChocoColor = FColor::FromHex("#D2691E");
		Txt_Team->SetColorAndOpacity(FSlateColor(ChocoColor));
	}
	else
	{
		TeamText = FText::FromString("Select Team!");

		FColor Color = FColor(0, 0, 0, 255);
		Txt_Team->SetColorAndOpacity(FSlateColor(Color));
	}

	Txt_Team->SetText(TeamText);

	if (APlayerController* PlayerController = PlayerState->GetPlayerController())
		IsLocalPlayer = PlayerController->IsLocalController();

	// Hide UI
	Btn_Ready->SetVisibility(ESlateVisibility::Hidden);
	Btn_KickPlayer->SetVisibility(ESlateVisibility::Hidden);
	Txt_Ready->SetVisibility(ESlateVisibility::Hidden);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Hidden);

	 if (IsLocalPlayer == false)
	 {
	 	//Txt_Team->SetVisibility(ESlateVisibility::Hidden);
		Btn_Mint->SetVisibility(ESlateVisibility::Hidden);
		Btn_Choco->SetVisibility(ESlateVisibility::Hidden);
	 }

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