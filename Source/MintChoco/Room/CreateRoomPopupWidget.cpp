// Fill out your copyright notice in the Description page of Project Settings.


#include "Room/CreateRoomPopupWidget.h"

#include "OnlineSessionsSubsystem.h"
#include "components/Button.h"
#include "Components/EditableTextBox.h"

void UCreateRoomPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OSS = GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();

	Btn_Create->OnClicked.AddDynamic(this, &UCreateRoomPopupWidget::OnCreateRoom);
	TxtBox_InputGameName->OnTextChanged.AddDynamic(this, &UCreateRoomPopupWidget::OnRoomNameChanged);

	// 팝업이 열릴 때는 보통 비어 있으므로 버튼이 꺼진 채 시작한다.
	UpdateCreateButton();
}

bool UCreateRoomPopupWidget::IsValidRoomName(const FText& Name)
{
	return !Name.ToString().TrimStartAndEnd().IsEmpty();
}

void UCreateRoomPopupWidget::OnRoomNameChanged(const FText& Text)
{
	UpdateCreateButton();
}

void UCreateRoomPopupWidget::UpdateCreateButton()
{
	// SetIsEnabled만 쓴다. 비활성 버튼도 자리에 그대로 보인다.
	Btn_Create->SetIsEnabled(IsValidRoomName(TxtBox_InputGameName->GetText()));
}

void UCreateRoomPopupWidget::OnCreateRoom()
{
	// 버튼이 꺼져 있어도 다른 경로(엔터 등)로 올 수 있으니 한 번 더 막는다.
	const FText Name = TxtBox_InputGameName->GetText();
	if (!IsValidRoomName(Name))
	{
		return;
	}

	OSS->OnMyCreateSession(Name.ToString().TrimStartAndEnd(), 100);
}
