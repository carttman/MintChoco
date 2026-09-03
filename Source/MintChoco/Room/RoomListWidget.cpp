// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/RoomListWidget.h"

#include "OnlineSessionsSubsystem.h"
#include "RoomItemWidget.h"
#include "Components/WrapBox.h"
#include "components/Button.h"


bool URoomListWidget::Initialize()
{
	if (Super::Initialize() == false)
			return false;

	//SetInfo();

	return true;
}

void URoomListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OSS = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();

	Btn_Refresh->OnClicked.AddDynamic(this, &URoomListWidget::OnMyFindRoom);
	OSS->OnSearchComplete.AddDynamic(this, &URoomListWidget::AddItemWidget);
}

// // 미리 50개정도 만들고 UI 갱신
// void URoomListWidget::SetInfo()
// {
// 	//Clear Children
// 	Rooms.Empty();
//
// 	//Create Child Widget
// 	for (int32 i = 0; i<50; i++)
// 	{
// 		if (RoomItemWidgetClass == nullptr)
// 			continue;
//
// 		URoomItemWidget* ChildWidget = CreateWidget<URoomItemWidget>(GetWorld(), RoomItemWidgetClass);
// 		if (ChildWidget == nullptr)
// 			continue;
//
// 		RoomList->AddChildToWrapBox(ChildWidget);
//
// 		Rooms.Add(ChildWidget);
// 	}
//
// 	RefreshUI();
// }
void URoomListWidget::RefreshUI()
{
	//Cache Session Length
	const int32 SessionLength = SessionInfos.Num();

	for (int32 i=0; i<Rooms.Num(); i++)
	{
		const int32 Index = i;

		if (Index < SessionLength)
		{
			//ShowUI
			Rooms[Index]->SetVisibility(ESlateVisibility::Visible);

			//FBlueprintSessionResult Result = SessionInfos[Index];
			auto Result = SessionInfos[Index];
			Rooms[Index]->SetInfo(Result);

		}
		else
		{
			//HideUI
			Rooms[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URoomListWidget::OnMyFindRoom()
{
	Rooms.Empty();
	RoomList->ClearChildren();

	UE_LOG(LogTemp, Warning, TEXT("URoomListWidget::OnMyFindRoom"));

	if (OSS)
	{
		OSS->OnMyFindSessions();
	}
}

void URoomListWidget::AddItemWidget(const struct FMySessionInfo& SessionInfo)
{
	URoomItemWidget* ItemWidget = CreateWidget<URoomItemWidget>(this, RoomItemWidgetClass);
	ItemWidget->SetInfo(SessionInfo);
	RoomList->AddChildToWrapBox(ItemWidget);

	Rooms.Add(ItemWidget);
}


