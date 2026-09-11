#include "Paint/PaintCoverageBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include "Game/GameGameState.h"
#include "Game/TeamTypes.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"

void FPaintCoverageBarMath::ComputeFills(float RawMint, float RawChoco, float& OutMint, float& OutChoco)
{
	OutMint = FMath::Clamp(RawMint, 0.0f, 1.0f);
	// The two fills grow towards each other and must never overlap, or the gap would stop meaning
	// "unpainted". Clamping Choco against the space Mint left keeps the reading honest while
	// replication catches up.
	OutChoco = FMath::Clamp(RawChoco, 0.0f, 1.0f - OutMint);
}

UPaintCoverageBarWidget::UPaintCoverageBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Font = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	Font.OutlineSettings.OutlineSize = 1;
}

TSharedRef<SWidget> UPaintCoverageBarWidget::RebuildWidget()
{
	// The bar needs no child widgets, only a root that gives the paint geometry a size. A UMG
	// subclass arrives with a designed tree and keeps it.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	return Super::RebuildWidget();
}

void UPaintCoverageBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Start on the real value so the bar does not sweep out from zero when the HUD appears.
	if (ReadCoverage(TargetMint, TargetChoco))
	{
		MintFraction = TargetMint;
		ChocoFraction = TargetChoco;
	}
}

bool UPaintCoverageBarWidget::ReadCoverage(float& OutMint, float& OutChoco) const
{
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	// The score is the server's grid, already measured and replicated. Only a map with no game
	// state falls back to this machine's own subsystem, which re-sums every cell grid on the spot -
	// hence the poll interval in NativeTick rather than a read per frame.
	const auto Take = [&OutMint, &OutChoco](const FPaintCoverage& Coverage)
	{
		OutMint = Coverage.GetFraction(static_cast<uint8>(Teams::Mint));
		OutChoco = Coverage.GetFraction(static_cast<uint8>(Teams::Choco));
	};

	if (const AGameGameState* const GameState = World->GetGameState<AGameGameState>())
	{
		Take(GameState->GetWorldCoverage());
		return true;
	}
	if (const UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>())
	{
		Take(Paint->GetWorldCoverage());
		return true;
	}
	return false;
}

void UPaintCoverageBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Coverage only changes as fast as it is measured, so reading it per frame would buy nothing
	// and, on the fallback path, would re-sum every cell grid every frame.
	PollTime += InDeltaTime;
	if (PollTime >= PollInterval)
	{
		PollTime = 0.0f;
		ReadCoverage(TargetMint, TargetChoco);
	}

	if (InterpSpeed <= 0.0f)
	{
		MintFraction = TargetMint;
		ChocoFraction = TargetChoco;
		return;
	}

	// The chase runs every frame even though the target steps, which is what turns the steps into
	// a bar that grows.
	MintFraction = FMath::FInterpConstantTo(MintFraction, TargetMint, InDeltaTime, InterpSpeed);
	ChocoFraction = FMath::FInterpConstantTo(ChocoFraction, TargetChoco, InDeltaTime, InterpSpeed);
}

int32 UPaintCoverageBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return Layer;
	}

	// One white brush tinted per rectangle: a bar is four solid rects, so no art is needed.
	const FSlateBrush* const Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const auto DrawRect = [&](int32 InLayer, const FVector2f& Offset, const FVector2f& Extent, const FLinearColor& Tint)
	{
		if (Extent.X <= 0.0f || Extent.Y <= 0.0f || Tint.A <= 0.0f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, InLayer,
			AllottedGeometry.ToPaintGeometry(Extent, FSlateLayoutTransform(FVector2f(Offset))),
			Brush, ESlateDrawEffect::None, Tint);
	};

	// The bar sits at the bottom of the slot; the labels take whatever is left above it, so the
	// slot's height is the one number a designer has to pick.
	const float Height = FMath::Min(BarHeight, Size.Y);
	const float Top = Size.Y - Height;
	const float Width = Size.X;

	float Mint = 0.0f;
	float Choco = 0.0f;
	FPaintCoverageBarMath::ComputeFills(MintFraction, ChocoFraction, Mint, Choco);

	if (BorderThickness > 0.0f && BorderColor.A > 0.0f)
	{
		DrawRect(Layer + 1, FVector2f(-BorderThickness, Top - BorderThickness),
			FVector2f(Width + BorderThickness * 2.0f, Height + BorderThickness * 2.0f), BorderColor);
	}

	DrawRect(Layer + 2, FVector2f(0.0f, Top), FVector2f(Width, Height), TrackColor);
	DrawRect(Layer + 3, FVector2f(0.0f, Top), FVector2f(Width * Mint, Height),
		FLinearColor(Teams::GetDisplayColor(Teams::Mint)));
	DrawRect(Layer + 3, FVector2f(Width * (1.0f - Choco), Top), FVector2f(Width * Choco, Height),
		FLinearColor(Teams::GetDisplayColor(Teams::Choco)));

	// Labels are measured rather than placed at a guessed offset, so "100.0 %" stays inside the bar.
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FString MintText = FString::Printf(TEXT("%s %.1f%%"), Teams::GetDisplayName(Teams::Mint), Mint * 100.0f);
	const FString ChocoText = FString::Printf(TEXT("%.1f%% %s"), Choco * 100.0f, Teams::GetDisplayName(Teams::Choco));
	const FVector2D ChocoExtent = Measure->Measure(ChocoText, Font);
	const float LabelTop = FMath::Max(Top - LabelPadding - static_cast<float>(ChocoExtent.Y), 0.0f);

	FSlateDrawElement::MakeText(OutDrawElements, Layer + 4,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(0.0f, LabelTop))),
		MintText, Font, ESlateDrawEffect::None, FLinearColor(Teams::GetDisplayColor(Teams::Mint)));
	FSlateDrawElement::MakeText(OutDrawElements, Layer + 4,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(Width - static_cast<float>(ChocoExtent.X), LabelTop))),
		ChocoText, Font, ESlateDrawEffect::None, FLinearColor(Teams::GetDisplayColor(Teams::Choco)));

	return Layer + 4;
}
