#include "Items/ItemLabelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UItemLabelWidget::SetLabel(const FText& InText)
{
	Text = InText;
	if (Label)
	{
		Label->SetText(Text);
	}
}

TSharedRef<SWidget> UItemLabelWidget::RebuildWidget()
{
	// UMG 서브클래스는 디자인된 트리를 들고 온다. 맨 C++ 클래스만 직접 짠다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	TSharedRef<SWidget> Built = Super::RebuildWidget();
	if (Label)
	{
		Label->SetText(Text);
	}
	return Built;
}

void UItemLabelWidget::BuildDefaultTree()
{
	Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetJustification(ETextJustify::Center);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));

	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 18;
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
	Label->SetFont(Font);

	WidgetTree->RootWidget = Label;
}
