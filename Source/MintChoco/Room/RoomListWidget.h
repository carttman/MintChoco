// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FindSessionsCallbackProxy.h"
#include "Blueprint/UserWidget.h"
#include "RoomListWidget.generated.h"

class UWrapBox;
class UButton;
class URoomItemWidget;

/**
 *
 */
UCLASS()
class MINTCHOCO_API URoomListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UFUNCTION(BlueprintCallable)
	void SetInfo();

	UFUNCTION(BlueprintCallable)
	void RefreshUI();

protected:
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UWrapBox> RoomList;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UButton> Btn_Refresh;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UButton> Btn_CreateGame;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UButton> Btn_Close;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<URoomItemWidget> RoomItemWidgetClass;

	UPROPERTY(BlueprintReadWrite)
	TArray<TObjectPtr<URoomItemWidget>> Rooms;

	UPROPERTY(BlueprintReadWrite)
	TArray<FBlueprintSessionResult> SessionInfos;
};
