#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "PaintChargeWidget.generated.h"

class UPaintWeaponComponent;

/**
 * A ring gauge around the crosshair that shows a Charged weapon's hold: a dim track is drawn the
 * whole way round and the bright arc fills clockwise from twelve o'clock as the charge builds, so
 * the shape itself says how far along the hold is. Reaching full flashes once and then holds full
 * brightness, which is the "release now" signal. Hidden while the trigger is not held or the
 * profile has no charge. Reads the owning player's pawn's weapon every tick, so a respawn or a
 * profile swap needs no rebinding. The ring is drawn in NativePaint, so no Blueprint asset is
 * required; a UMG subclass may add a tree of its own on top.
 */
UCLASS()
class MINTCHOCO_API UPaintChargeWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "1", ForceUnits = "px"))
	float Radius = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float Thickness = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	FLinearColor Color = FLinearColor(1.0f, 0.78f, 0.1f);

	/** Where the fill starts, clockwise from twelve o'clock. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ForceUnits = "deg"))
	float StartAngleDeg = 0.0f;

	/** False fills anticlockwise. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	bool bClockwise = true;

	/**
	 * The unfilled remainder of the ring, drawn dim under the fill. Without it a short arc floating
	 * in space reads as a stray mark rather than as a gauge that is barely started. 0 hides it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Track", meta = (ClampMin = "0", ClampMax = "1"))
	float TrackOpacity = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Track", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float TrackThickness = 2.0f;

	/** How long the one flash on reaching full lasts. The ring holds full brightness afterwards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Full", meta = (ClampMin = "0", ForceUnits = "s"))
	float FullPulseTime = 0.35f;

	/** Peak brightness of that flash, as a multiple of the lit ring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Full", meta = (ClampMin = "1", ClampMax = "4", ForceUnits = "x"))
	float FullPulseIntensity = 1.8f;

	/** Halo rings drawn under the core, each wider and fainter than the last. 0 draws no glow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0", ClampMax = "4"))
	int32 GlowLayers = 3;

	/** Alpha of the innermost halo at full brightness. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.35f;

	/** How far the halos reach, as a multiple of the core thickness doubling per layer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0.5", ClampMax = "4"))
	float GlowSpread = 1.0f;

	/** Segments in a full circle. An arc uses the matching fraction of them, so density is constant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "8"))
	int32 Segments = 48;

private:
	/** The highest charge among the pawn's weapons: whichever trigger is held with a Charged profile owns the ring. */
	float FindChargeFraction() const;

	/** Ring points from the start angle sweeping Fraction of the way round. Fraction 1 closes the circle. */
	void BuildArc(const FVector2f& Center, float Fraction, TArray<FVector2f>& OutPoints) const;

	/** Seconds since the charge reached full; drives the one flash and resets when the hold ends. */
	float FullTime = 0.0f;
	float ChargeFraction = 0.0f;
};
