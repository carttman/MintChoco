#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "PaintChargeWidget.generated.h"

class UPaintWeaponComponent;

/**
 * A ring around the crosshair that shows a Charged weapon's hold: it blinks while the charge
 * builds and stays lit once releasing would fire. Hidden while the trigger is not held or the
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

	/** One dark-bright-dark breath of the ring while the charge builds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float BlinkPeriod = 1.0f;

	/** Brightness at the bottom of a breath. 0 fades out completely. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0", ClampMax = "1"))
	float PulseMinIntensity = 0.1f;

	/** Halo rings drawn under the core, each wider and fainter than the last. 0 draws no glow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0", ClampMax = "4"))
	int32 GlowLayers = 3;

	/** Alpha of the innermost halo at full brightness. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.35f;

	/** How far the halos reach, as a multiple of the core thickness doubling per layer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge|Glow", meta = (ClampMin = "0.5", ClampMax = "4"))
	float GlowSpread = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "8"))
	int32 Segments = 48;

private:
	/** The highest charge among the pawn's weapons: whichever trigger is held with a Charged profile owns the ring. */
	float FindChargeFraction() const;

	/** Seconds the current charge has been building; drives the blink phase and resets when the hold ends. */
	float ChargingTime = 0.0f;
	float ChargeFraction = 0.0f;
};
