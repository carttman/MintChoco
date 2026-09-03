// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/RoomItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"


// void URoomItemWidget::SetInfo(FBlueprintSessionResult InSessionResult)
// {
// 	//Result = InSessionResult;
// 	RefreshUI();
// }

void URoomItemWidget::SetInfo(const struct FMySessionInfo& SessionInfo)
{
	Result = SessionInfo;
	RefreshUI();
}

void URoomItemWidget::RefreshUI()
{

	//FString RoomName;
	//Result.OnlineResult.Session.SessionSettings.Get(FName("ROOM_NAME"), OUT RoomName);
	Txt_RoomName->SetText(FText::FromString(Result.RoomName));

	//FString MapName;
	//Result.OnlineResult.Session.SessionSettings.Get(FName("MAP_NAME"), OUT MapName);
	//Txt_MapName->SetText(FText::FromString(Result.));

	FString PlayerCountString = FString::Printf(TEXT("( %d / %d )"), Result.JoinPlayerCount, Result.MaxPlayer);
	Txt_PlayerCount->SetText(FText::FromString(PlayerCountString));
	//const int32 MaxPlayers = Result.OnlineResult.Session.SessionSettings.NumPublicConnections;
	//const int32 CurrentPlayers = Result.OnlineResult.Session.SessionSettings.NumPublicConnections - Result.OnlineResult.Session.NumOpenPublicConnections;
	//const int32 MaxPlayers = Result.OnlineResult.Session.SessionSettings.NumPublicConnections;
	//FString PlayerCountString = FString::Printf(TEXT("( %d / %d )"), CurrentPlayers, MaxPlayers);
	//Txt_PlayerCount->SetText(FText::FromString(PlayerCountString));
}
