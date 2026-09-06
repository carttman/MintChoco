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

	// Show Kick Button
	if (IsServer && IsLocalPlayer == false)
		Btn_KickPlayer->SetVisibility(ESlateVisibility::Visible);

	// Set Nickname
	Editable_PlayerName->SetText(PlayerState->Nickname);
	Editable_PlayerName->SetVisibility(ESlateVisibility::Visible);

	// Txt_PlayerName->SetText(PlayerState->Nickname);
	// Txt_PlayerName->SetVisibility(ESlateVisibility::Visible);
}