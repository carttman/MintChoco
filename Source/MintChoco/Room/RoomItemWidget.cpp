// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/RoomItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"


void URoomItemWidget::SetInfo(FBlueprintSessionResult InSessionResult)
{
	Result = InSessionResult;
	RefreshUI();
}

void URoomItemWidget::RefreshUI()
{
	Txt_RoomName->SetText(FText());

	Txt_MapName->SetText(FText());

	Txt_PlayerCount->SetText(FText());
}
