#include "Sample/SampleCoverageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

#include "Paint/PaintDebugDraw.h"
#include "Paint/PaintSubsystem.h"

TSharedRef<SWidget> USampleCoverageWidget::RebuildWidget()
{
	// A UMG subclass arrives with a designed tree; only the bare C++ class builds its own.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	return Super::RebuildWidget();
}

void USampleCoverageWidget::BuildDefaultTree()
{
	UCanvasPanel* const Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* const RowsSlot = Root->AddChildToCanvas(Rows);
	RowsSlot->SetAutoSize(true);
	RowsSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	RowsSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	RowsSlot->SetPosition(FVector2D(-40.0f, 40.0f));

	UTextBlock* const Header = WidgetTree->ConstructWidget<UTextBlock>();
	Header->SetText(FText::FromString(TEXT("Coverage")));
	Header->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
}

void USampleCoverageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Rows && RowTexts.IsEmpty())
	{
		BuildRows();
	}
}

void USampleCoverageWidget::BuildRows()
{
	for (int32 Id = 0; Id < PaintIdCount; ++Id)
	{
		UTextBlock* const Row = WidgetTree->ConstructWidget<UTextBlock>();
		Row->SetColorAndOpacity(FSlateColor(FLinearColor(PaintDebug::IdColor(static_cast<uint8>(Id)))));
		Row->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Rows->AddChildToVerticalBox(Row);
		RowTexts.Add(Row);
	}
}

void USampleCoverageWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* const World = GetWorld();
	const UPaintSubsystem* const Coverage = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
	if (Coverage && RowTexts.Num() == PaintIdCount)
	{
		Refresh(Coverage->GetWorldCoverage());
	}
}

void USampleCoverageWidget::Refresh(const FPaintCoverage& Coverage)
{
	for (int32 Id = 0; Id < PaintIdCount; ++Id)
	{
		UTextBlock* const Row = RowTexts[Id];
		const float Percent = Coverage.GetFraction(static_cast<uint8>(Id)) * 100.0f;

		// Player teams and the unpainted remainder always show; the reserved ids only once used.
		const bool bAlwaysShown = Id < 4 || Id == PaintIdNone;
		Row->SetVisibility(bAlwaysShown || Coverage.AreaByPaintId[Id] > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		const FString Label = Id == PaintIdNone ? TEXT("Unpainted") : FString::Printf(TEXT("Team %d"), Id);
		Row->SetText(FText::FromString(FString::Printf(TEXT("%s   %5.1f %%"), *Label, Percent)));
	}
}
