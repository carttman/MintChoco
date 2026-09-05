#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "SampleCoverageWidget.generated.h"

class UTextBlock;
class UVerticalBox;
struct FPaintCoverage;

/**
 * World coverage readout for the sample map: one row per paint id with its share of every
 * paintable surface, polled from UPaintCoverageSubsystem. Builds its own tree in C++ like the
 * seed widget; a UMG subclass can restyle it by providing a VerticalBox named Rows.
 */
UCLASS()
class MINTCHOCO_API USampleCoverageWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> Rows;

private:
	void BuildDefaultTree();
	void BuildRows();
	void Refresh(const FPaintCoverage& Coverage);

	/** One per paint id, PaintIdNone last as the unpainted remainder. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RowTexts;
};
