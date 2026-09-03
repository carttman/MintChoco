// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CreateRoomPopupWidget.generated.h"

/**
 *
 */
UCLASS()
class MINTCHOCO_API UCreateRoomPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TObjectPtr<class UOnlineSessionsSubsystem> GI;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UEditableTextBox> TxtBox_InputGameName;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UButton> Btn_Create;

public:
	UFUNCTION()
	void OnCreateRoom();
};
