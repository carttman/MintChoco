#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

#include "SamplePaintController.generated.h"

class UInputAction;
class UInputMappingContext;
class USampleSeedWidget;
class UUserWidget;

/**
 * Debug paint source for the sample map: traces along the crosshair and emits one splat per click.
 *
 * This is the simplest possible producer of an FPaintSplat. A paintball projectile or a mop fills
 * the same struct from its own impact, so adding them does not change anything downstream - it
 * only adds another source. That is why this controller stays useful as a debug tool afterwards.
 */
UCLASS()
class MINTCHOCO_API ASamplePaintController : public APlayerController
{
	GENERATED_BODY()

public:
	ASamplePaintController();

	/** Debug seed control: pin every splat to one seed, or return to a fresh seed per splat. */
	UFUNCTION(BlueprintCallable, Category = "Sample|Paint")
	void SetSeedOverride(bool bInUseFixedSeed, int32 InFixedSeed);

	UFUNCTION(BlueprintPure, Category = "Sample|Paint")
	bool IsUsingFixedSeed() const { return bUseFixedSeed; }

	/** Seed the next splat will use. The debug widget mirrors this value. */
	UFUNCTION(BlueprintPure, Category = "Sample|Paint")
	int32 GetNextSeed() const { return NextSeed; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void OnPaintTriggered();
	void OnContinuousPaintTriggered();
	void OnContinuousPaintReleased();
	void OnCycleTeamTriggered(const FInputActionValue& Value);
	void OnToggleUIFocus();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputMappingContext> PaintMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> PaintAction;

	/** Boolean action held down to paint a continuous stroke, as a mop would. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> ContinuousPaintAction;

	/** Axis1D action (mouse wheel) that steps TeamId through 1..NumTeams. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> CycleTeamAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|UI")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	/** Defaults to the C++ widget, which builds its own layout; a UMG subclass restyles it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|UI")
	TSubclassOf<USampleSeedWidget> SeedWidgetClass;

	/** Paint id written by the next click. 0-3 are player teams; 7 (PaintIdNone) erases. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 TeamId = 0;

	/** The wheel cycles TeamId through 0..NumTeams-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint", meta = (ClampMin = "1", ClampMax = "7"))
	uint8 NumTeams = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint")
	float TraceDistance = 10000.0f;

	/**
	 * Speed written into the splat's incident velocity. A hitscan trace has no real speed of its
	 * own, so the sample fakes one; a projectile will pass its actual impact velocity instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint")
	float NominalImpactSpeed = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint")
	float SplatVolume = 1.0f;

	/**
	 * World-space distance the aim point must travel before a held stroke deposits the next splat.
	 * Every splat costs a full-target draw, so spacing them is what keeps a stroke affordable;
	 * too large and the stroke breaks into separate blobs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint", meta = (ClampMin = "0"))
	float ContinuousSpacing = 15.0f;

	/** Draws the trace and the surface normal for a few seconds on every click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Debug")
	bool bDrawDebugTrace = false;

private:
	bool TracePaintTarget(FHitResult& OutHit, FVector& OutDirection) const;
	bool PaintAtHit(const FHitResult& Hit, const FVector& Direction);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<USampleSeedWidget> SeedWidget;

	FVector StrokeAnchor = FVector::ZeroVector;
	bool bStrokeAnchorValid = false;
	bool bUIFocused = false;
	bool bUseFixedSeed = false;
	int32 NextSeed = 0;
};
