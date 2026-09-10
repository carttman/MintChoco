// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CreateRoomPopupWidget.generated.h"

/**
 * 방 만들기 팝업. 제목을 입력해야 만들기 버튼이 눌린다.
 *
 * 버튼은 숨기지 않고 비활성화만 한다: 자리는 그대로 있고 회색으로 보인다.
 */
UCLASS()
class MINTCHOCO_API UCreateRoomPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TObjectPtr<class UOnlineSessionsSubsystem> OSS;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UEditableTextBox> TxtBox_InputGameName;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> Btn_Create;

public:
	UFUNCTION()
	void OnCreateRoom();

	/** 공백만 있는 제목은 빈 제목이다. */
	static bool IsValidRoomName(const FText& Name);

private:
	UFUNCTION()
	void OnRoomNameChanged(const FText& Text);

	/** 제목이 유효할 때만 만들기 버튼을 켠다. 보이기는 건드리지 않는다. */
	void UpdateCreateButton();
};
