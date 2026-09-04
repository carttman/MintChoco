// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyUserWidget.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LobbyWidget.h"

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
	bool IsReady = PlayerState->Ready;
	bool IsServer = UKismetSystemLibrary::IsServer(this);
	bool IsLocalPlayer = false;
	FText TeamText;

	if (PlayerState->Team == 0)
	{
		TeamText = FText::FromString("Mint");

		FColor MintColor = FColor(62, 180, 137, 255);
		Txt_Team->SetColorAndOpacity(FSlateColor(MintColor));
	}
	else
	{
		TeamText = FText::FromString("Choco");

		FColor ChocoColor = FColor::FromHex("#D2691E");
		Txt_Team->SetColorAndOpacity(FSlateColor(ChocoColor));
	}


	Txt_Team->SetText(TeamText);

	if (APlayerController* PlayerController = PlayerState->GetPlayerController())
		IsLocalPlayer = PlayerController->IsLocalController();

	// Hide UI
	Btn_Ready->SetVisibility(ESlateVisibility::Hidden);
	Btn_KickPlayer->SetVisibility(ESlateVisibility::Hidden);
	Txt_Ready->SetVisibility(ESlateVisibility::Hidden);
	Txt_PlayerName->SetVisibility(ESlateVisibility::Hidden);
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

	// Show Kick Button
	if (IsServer && IsLocalPlayer == false)
		Btn_KickPlayer->SetVisibility(ESlateVisibility::Visible);

	// Set Nickname
	Editable_PlayerName->SetText(PlayerState->Nickname);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Visible);

	if (IsReady)
		Editable_PlayerName->SetIsReadOnly(true);
	else
		Editable_PlayerName->SetIsReadOnly(!IsLocalPlayer);
}