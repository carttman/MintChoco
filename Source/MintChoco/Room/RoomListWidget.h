// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionsSubsystem.h"
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

protected:
	virtual void NativeConstruct() override;

	// UFUNCTION(BlueprintCallable)
	// void SetInfo();

	// UFUNCTION(BlueprintCallable)
	// void RefreshUI();

private:
	UFUNCTION()
	void OnMyFindRoom();

	UFUNCTION()
	void AddItemWidget(const struct FMySessionInfo& SessionInfo);

	UFUNCTION()
	void OnSetRefreshBtn(bool flag);
protected:
	UPROPERTY()
	TObjectPtr<class UOnlineSessionsSubsystem> OSS;

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
	TArray<FMySessionInfo> SessionInfos;


};
