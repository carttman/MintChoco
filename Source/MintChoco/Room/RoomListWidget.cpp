// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/RoomListWidget.h"
#include "RoomItemWidget.h"
#include "Components/WrapBox.h"
#include "components/Button.h"


bool URoomListWidget::Initialize()
{
	if (Super::Initialize() == false)
			return false;

	SetInfo();

	return true;
}

void URoomListWidget::SetInfo()
{
	//Clear Children
	Rooms.Empty();

	//Create Child Widget
	for (int32 i = 0; i<50; i++)
	{
		if (RoomItemWidgetClass == nullptr)
			continue;

		URoomItemWidget* ChildWidget = CreateWidget<URoomItemWidget>(GetWorld(), RoomItemWidgetClass);
		if (ChildWidget == nullptr)
			continue;

		RoomList->AddChildToWrapBox(ChildWidget);

		Rooms.Add(ChildWidget);
	}

	RefreshUI();
}

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

			FBlueprintSessionResult Result = SessionInfos[Index];
			Rooms[Index]->SetInfo(Result);

		}
		else
		{
			//HideUI
			Rooms[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}