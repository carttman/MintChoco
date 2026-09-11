#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "ItemLabelWidget.generated.h"

class UTextBlock;

/**
 * 픽업 위에 떠서 아이템 이름을 보여 주는 이름표. 트리를 C++로 직접 짜므로 블루프린트 에셋이
 * 필요 없고, UMG 서브클래스가 Label이라는 TextBlock을 두면 그 모양을 대신 쓴다.
 */
UCLASS()
class MINTCHOCO_API UItemLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 보여 줄 이름. 트리가 아직 없으면 기억해 두고 만들어질 때 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetLabel(const FText& InText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

private:
	void BuildDefaultTree();

	FText Text;
};
