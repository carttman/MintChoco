// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/RoomListWidget.h"

#include "OnlineSessionsSubsystem.h"
#include "RoomItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "components/Button.h"


void URoomListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OSS = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();

	Btn_Refresh->OnClicked.AddDynamic(this, &URoomListWidget::OnMyFindRoom);
	if (OSS)
	{
		OSS->OnSearchComplete.AddDynamic(this, &URoomListWidget::AddItemWidget);
		OSS->OnSearchLockComplete.AddDynamic(this, &URoomListWidget::OnSetRefreshBtn);
	}

	// 검색 전에는 문구도 없다. 검색이 시작되면 OnSetRefreshBtn이 켠다.
	if (Txt_Searching)
	{
		Txt_Searching->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 방 목록에 들어오면 새로고침을 누르지 않아도 한 번 찾는다.
	OnMyFindRoom();
}

void URoomListWidget::NativeDestruct()
{
	// 서브시스템은 맵을 넘어 살아남는다. 풀지 않으면 다음에 열린 목록에 지난 목록의 바인딩이 남아
	// 사라진 위젯으로 결과가 간다.
	if (OSS)
	{
		OSS->OnSearchComplete.RemoveDynamic(this, &URoomListWidget::AddItemWidget);
		OSS->OnSearchLockComplete.RemoveDynamic(this, &URoomListWidget::OnSetRefreshBtn);
	}

	Super::NativeDestruct();
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
// void URoomListWidget::RefreshUI()
// {
// 	//Cache Session Length
// 	const int32 SessionLength = SessionInfos.Num();
//
// 	for (int32 i=0; i<Rooms.Num(); i++)
// 	{
// 		const int32 Index = i;
//
// 		if (Index < SessionLength)
// 		{
// 			//ShowUI
// 			Rooms[Index]->SetVisibility(ESlateVisibility::Visible);
//
// 			//FBlueprintSessionResult Result = SessionInfos[Index];
// 			auto Result = SessionInfos[Index];
// 			Rooms[Index]->SetInfo(Result);
//
// 		}
// 		else
// 		{
// 			//HideUI
// 			Rooms[Index]->SetVisibility(ESlateVisibility::Collapsed);
// 		}
// 	}
// }

void URoomListWidget::OnMyFindRoom()
{
	Rooms.Empty();
	RoomList->ClearChildren();
	//Btn_Refresh->SetIsEnabled(false);
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


void URoomListWidget::OnSetRefreshBtn(bool bSearching)
{
	Btn_Refresh->SetIsEnabled(!bSearching);

	// 목록은 검색을 시작할 때 비워지므로, 문구가 빈 자리를 채우고 결과가 오면 사라진다.
	if (Txt_Searching)
	{
		Txt_Searching->SetVisibility(bSearching ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}


