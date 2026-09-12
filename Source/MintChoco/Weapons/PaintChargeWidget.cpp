#include "Weapons/PaintChargeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

#include "Weapons/PaintWeaponComponent.h"

/**
 * Forces the gauge to a fixed fill so its look can be checked without holding a trigger: a
 * screenshot is enough. Negative reads the real charge, which is the shipping behaviour.
 */
static TAutoConsoleVariable<float> CVarChargeRingPreview(
	TEXT("mc.ChargeRingPreview"),
	-1.0f,
	TEXT("0..1 로 차지 게이지를 강제로 채워 그린다. 음수면 실제 차지값을 쓴다."),
	ECVF_Cheat);

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
	const float Preview = CVarChargeRingPreview.GetValueOnGameThread();
	if (Preview >= 0.0f)
	{
		return FMath::Clamp(Preview, 0.0f, 1.0f);
	}

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
	FullTime = ChargeFraction >= 1.0f ? FullTime + InDeltaTime : 0.0f;
}

void UPaintChargeWidget::BuildArc(const FVector2f& Center, float Fraction, TArray<FVector2f>& OutPoints) const
{
	// Slate's Y grows downward, so twelve o'clock is -Y and a clockwise sweep leans towards +X.
	const float Sweep = 2.0f * UE_PI * FMath::Clamp(Fraction, 0.0f, 1.0f);
	const float Start = FMath::DegreesToRadians(StartAngleDeg);
	const float Direction = bClockwise ? 1.0f : -1.0f;

	// A partial arc keeps the same angular density as the full ring, so a nearly empty gauge is not
	// drawn as one coarse chord.
	const int32 Count = FMath::Max(FMath::CeilToInt(static_cast<float>(Segments) * Fraction), 1);
	const float Step = Sweep / static_cast<float>(Count);

	OutPoints.Reset(Count + 1);
	for (int32 Index = 0; Index <= Count; ++Index)
	{
		const float Angle = Start + Direction * Step * static_cast<float>(Index);
		OutPoints.Add(Center + FVector2f(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius);
	}
}

int32 UPaintChargeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (ChargeFraction <= 0.0f)
	{
		return Layer;
	}

	const FVector2f Center = AllottedGeometry.GetLocalSize() * 0.5f;
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();

	// The unfilled remainder, so the gauge reads as a ring being filled rather than a lone arc.
	if (TrackOpacity > UE_KINDA_SMALL_NUMBER)
	{
		TArray<FVector2f> TrackPoints;
		BuildArc(Center, 1.0f, TrackPoints);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, Geometry, MoveTemp(TrackPoints),
			ESlateDrawEffect::None, FLinearColor(Color.R, Color.G, Color.B, Color.A * TrackOpacity), /*bAntialias=*/true, TrackThickness);
	}

	// Building holds steady brightness - the arc's length is what changes. Reaching full flashes
	// once and decays back to lit, which is the moment the shot becomes available.
	const float Pulse = FullPulseTime > 0.0f ? FMath::Clamp(1.0f - FullTime / FullPulseTime, 0.0f, 1.0f) : 0.0f;
	const float Intensity = FMath::Lerp(1.0f, FullPulseIntensity, Pulse);

	TArray<FVector2f> Points;
	BuildArc(Center, ChargeFraction, Points);

	// Slate has no bloom, so the glow is the same arc drawn wider and fainter underneath the core:
	// each halo doubles the width and quarters the alpha, which reads as light bleeding outward.
	for (int32 Halo = GlowLayers; Halo >= 1; --Halo)
	{
		const float Width = Thickness * FMath::Pow(2.0f, static_cast<float>(Halo)) * GlowSpread;
		const float Alpha = FMath::Min(GlowStrength * Intensity / FMath::Pow(4.0f, static_cast<float>(Halo - 1)), 1.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, Geometry, Points,
			ESlateDrawEffect::None, FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha), /*bAntialias=*/true, Width);
	}

	// The core brightens past the base color on the flash, so full charge reads as lit, not just opaque.
	const FLinearColor Core = Color * Intensity;
	FSlateDrawElement::MakeLines(OutDrawElements, Layer + 3, Geometry, MoveTemp(Points),
		ESlateDrawEffect::None, FLinearColor(Core.R, Core.G, Core.B, Color.A), /*bAntialias=*/true, Thickness);
	return Layer + 3;
}
