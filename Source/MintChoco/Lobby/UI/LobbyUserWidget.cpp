// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/UI/LobbyUserWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Lobby/Contents/LobbyPlayerState.h"

void ULobbyUserWidget::SetInfo(ALobbyPlayerState* InPlayerState)
{
	PlayerState = InPlayerState;

	ReFreshUI();
}

void ULobbyUserWidget::ReFreshUI()
{
	if (PlayerState == nullptr)
		return;

	// Cache Local Variables
	bool IsLocalPlayer = false;
	bool IsServer = UKismetSystemLibrary::IsServer(this);
	bool IsReady = PlayerState->Ready;

	if (APlayerController* PlayerController= PlayerState->GetPlayerController())
		IsLocalPlayer = PlayerController->IsLocalController();


	// Hide UI
	Btn_Ready->SetVisibility(ESlateVisibility::Hidden);
	Btn_KickPlayer->SetVisibility(ESlateVisibility::Hidden);
	Txt_Ready->SetVisibility(ESlateVisibility::Hidden);
	Txt_PlayerName->SetVisibility(ESlateVisibility::Hidden);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Hidden);


	// Show Ready Text
	if (IsReady)
		Txt_Ready->SetVisibility(ESlateVisibility::Visible);

	// Show Ready Button
	if (IsLocalPlayer && IsReady == false)
		Btn_Ready->SetVisibility(ESlateVisibility::Visible);

	if (IsServer && IsLocalPlayer == false)
		Btn_KickPlayer->SetVisibility(ESlateVisibility::Visible);

	//set Nickname
	Editable_PlayerName->SetText(PlayerState->Nickname);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Visible);


	if (IsReady)
		Editable_PlayerName->SetIsReadOnly(true);
	else
		Editable_PlayerName->SetIsReadOnly(!IsLocalPlayer);


}