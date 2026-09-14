// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FindSessionsCallbackProxy.h"
#include "OnlineSessionsSubsystem.h"
#include "UI/MenuWidget.h"
#include "RoomItemWidget.generated.h"

class UTextBlock;
class UButton;

/**
 *
 */
UCLASS()
class MINTCHOCO_API URoomItemWidget : public UMenuWidget
{
	GENERATED_BODY()
private:

	virtual void NativeConstruct() override;

	/** 참가 버튼은 자기 소리(Audio.UI.Join)를 내므로 자동 클릭음에서 뺀다. */
	virtual bool ShouldAutoSound(const UButton* Button) const override { return Button != Btn_Join.Get(); }
public:
	// UFUNCTION(BlueprintCallable)
	// void SetInfo(FBlueprintSessionResult InSessionResult);

	UFUNCTION(BlueprintCallable)
	void SetInfo(const struct FMySessionInfo& SessionInfo);

	UFUNCTION(BlueprintCallable)
	void RefreshUI();

	UFUNCTION()
	void OnTryJoinSession();
protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RoomName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PlayerCount;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_MapName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Join;

public:
	UPROPERTY(BlueprintReadOnly)
	FMySessionInfo Result;

	int32 SessionSearchIndex;

};
