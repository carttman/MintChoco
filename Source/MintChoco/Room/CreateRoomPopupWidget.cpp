// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/CreateRoomPopupWidget.h"

#include "OnlineSessionsSubsystem.h"
#include "components/Button.h"
#include "Components/EditableTextBox.h"

void UCreateRoomPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GI = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();

	Btn_Create->OnClicked.AddDynamic(this, &UCreateRoomPopupWidget::OnCreateRoom);

}

void UCreateRoomPopupWidget::OnCreateRoom()
{
	if (false == TxtBox_InputGameName->GetText().IsEmpty())
	{
		GI->MySessionName = TxtBox_InputGameName->GetText().ToString();
	}

	GI->OnMyCreateSession(
		TxtBox_InputGameName->GetText().ToString(),
		100);
}
