// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "RoomListWidget.generated.h"

class UWrapBox;
class UButton;
class UTextBlock;
class URoomItemWidget;

/**
 * 방 목록. 열리는 순간 한 번 검색하고, 새로고침 버튼으로 다시 검색한다.
 *
 * 검색 중에는 새로고침 버튼을 잠그고 "찾는 중" 문구를 띄운다. 그 신호는 세션
 * 서브시스템의 OnSearchLockComplete 하나에서 오므로, 검색을 누가 시작했든 표시가 맞는다.
 */
UCLASS()
class MINTCHOCO_API URoomListWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	/** 검색 중에만 보이는 문구. 위젯에 없으면 버튼 잠금만 한다. */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Searching;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<URoomItemWidget> RoomItemWidgetClass;

	UPROPERTY(BlueprintReadWrite)
	TArray<TObjectPtr<URoomItemWidget>> Rooms;

	UPROPERTY(BlueprintReadWrite)
	TArray<FMySessionInfo> SessionInfos;


};
