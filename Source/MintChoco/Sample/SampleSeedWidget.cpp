#include "Sample/SampleSeedWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

#include "Sample/SamplePaintController.h"

void USampleSeedWidget::SetDisplayedSeed(int32 Seed)
{
	if (SeedInput && !SeedInput->HasKeyboardFocus())
	{
		SeedInput->SetText(FText::FromString(FString::FromInt(Seed)));
	}
}

TSharedRef<SWidget> USampleSeedWidget::RebuildWidget()
{
	// A UMG subclass arrives with a designed tree; only the bare C++ class builds its own.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	return Super::RebuildWidget();
}

void USampleSeedWidget::BuildDefaultTree()
{
	UCanvasPanel* const Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UCanvasPanelSlot* const RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAutoSize(true);
	RowSlot->SetPosition(FVector2D(40.0f, 40.0f));

	UTextBlock* const SeedLabel = WidgetTree->ConstructWidget<UTextBlock>();
	SeedLabel->SetText(FText::FromString(TEXT("Seed")));
	Row->AddChildToHorizontalBox(SeedLabel)->SetPadding(FMargin(0.0f, 4.0f, 8.0f, 0.0f));

	SeedInput = WidgetTree->ConstructWidget<UEditableTextBox>();
	SeedInput->SetText(FText::FromString(TEXT("0")));
	Row->AddChildToHorizontalBox(SeedInput)->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));

	AutoSeedCheck = WidgetTree->ConstructWidget<UCheckBox>();
	AutoSeedCheck->SetIsChecked(true);
	Row->AddChildToHorizontalBox(AutoSeedCheck)->SetPadding(FMargin(0.0f, 4.0f, 4.0f, 0.0f));

	UTextBlock* const AutoLabel = WidgetTree->ConstructWidget<UTextBlock>();
	AutoLabel->SetText(FText::FromString(TEXT("Auto reseed")));
	Row->AddChildToHorizontalBox(AutoLabel)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
}

void USampleSeedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const ASamplePaintController* const Controller = Cast<ASamplePaintController>(GetOwningPlayer()))
	{
		SetDisplayedSeed(Controller->GetNextSeed());
		if (AutoSeedCheck)
		{
			AutoSeedCheck->SetIsChecked(!Controller->IsUsingFixedSeed());
		}
	}

	if (SeedInput)
	{
		SeedInput->OnTextCommitted.AddUniqueDynamic(this, &USampleSeedWidget::OnSeedCommitted);
	}
	if (AutoSeedCheck)
	{
		AutoSeedCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USampleSeedWidget::OnAutoSeedChanged);
	}
}

void USampleSeedWidget::OnSeedCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	// Typing a seed is an implicit switch to pinned mode; the checkbox switches back.
	if (CommitType != ETextCommit::OnCleared && AutoSeedCheck)
	{
		AutoSeedCheck->SetIsChecked(false);
	}
	PushToController();
}

void USampleSeedWidget::OnAutoSeedChanged(bool bIsChecked)
{
	PushToController();
}

void USampleSeedWidget::PushToController()
{
	if (ASamplePaintController* const Controller = Cast<ASamplePaintController>(GetOwningPlayer()))
	{
		const bool bAuto = !AutoSeedCheck || AutoSeedCheck->IsChecked();
		const int32 Seed = SeedInput ? FCString::Atoi(*SeedInput->GetText().ToString()) : 0;
		Controller->SetSeedOverride(!bAuto, Seed);
	}
}
