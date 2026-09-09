#include "Weapons/PaintChargeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

#include "Weapons/PaintWeaponComponent.h"

TSharedRef<SWidget> UPaintChargeWidget::RebuildWidget()
{
	// The ring needs no child widgets, only a root that fills the viewport so the paint geometry
	// has a center. A UMG subclass arrives with a designed tree and keeps it.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	return Super::RebuildWidget();
}

void UPaintChargeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

float UPaintChargeWidget::FindChargeFraction() const
{
	const APlayerController* const PlayerController = GetOwningPlayer();
	const APawn* const Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return 0.0f;
	}

	float Fraction = 0.0f;
	Pawn->ForEachComponent<UPaintWeaponComponent>(/*bIncludeFromChildActors=*/false,
		[&Fraction](const UPaintWeaponComponent* Weapon) { Fraction = FMath::Max(Fraction, Weapon->GetChargeFraction()); });
	return Fraction;
}

void UPaintChargeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ChargeFraction = FindChargeFraction();
	ChargingTime = ChargeFraction > 0.0f ? ChargingTime + InDeltaTime : 0.0f;
}

int32 UPaintChargeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (ChargeFraction <= 0.0f)
	{
		return Layer;
	}

	// Building: the ring breathes, fading up and down on a cosine. Full: it holds full brightness,
	// which is the "release now" signal.
	const bool bFull = ChargeFraction >= 1.0f;
	const float Pulse = 0.5f - 0.5f * FMath::Cos(2.0f * UE_PI * ChargingTime / BlinkPeriod);
	const float Intensity = bFull ? 1.0f : FMath::Lerp(PulseMinIntensity, 1.0f, Pulse);
	if (Intensity <= UE_KINDA_SMALL_NUMBER)
	{
		return Layer;
	}

	const FVector2f Center = AllottedGeometry.GetLocalSize() * 0.5f;
	TArray<FVector2f> Points;
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / static_cast<float>(Segments);
		Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}

	// Slate has no bloom, so the glow is the same ring drawn wider and fainter underneath the core:
	// each halo doubles the width and quarters the alpha, which reads as light bleeding outward.
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	for (int32 Halo = GlowLayers; Halo >= 1; --Halo)
	{
		const float Width = Thickness * FMath::Pow(2.0f, static_cast<float>(Halo)) * GlowSpread;
		const float Alpha = GlowStrength * Intensity / FMath::Pow(4.0f, static_cast<float>(Halo - 1));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, Geometry, Points,
			ESlateDrawEffect::None, FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha), /*bAntialias=*/true, Width);
	}

	// The core brightens past the base color as it fills, so full charge reads as lit, not just opaque.
	const FLinearColor Core = Color * FMath::Lerp(0.6f, 1.4f, Intensity);
	FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, Geometry, MoveTemp(Points),
		ESlateDrawEffect::None, FLinearColor(Core.R, Core.G, Core.B, Color.A * Intensity), /*bAntialias=*/true, Thickness);
	return Layer + 2;
}
