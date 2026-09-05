#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

#include "SamplePaintController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPaintBrushProfile;
class UPaintSubsystem;
class UPaintWeaponComponent;
class UPaintWeaponProfile;
class USampleCoverageWidget;
class USampleSeedWidget;

/**
 * Debug paint source for the sample map: traces along the crosshair and emits one splat per click.
 *
 * This is the simplest possible producer of an FPaintSplat. It holds a brush profile like a
 * paintball gun or a mop would, asks it to build the splat from the hit, and hands the result to
 * the paint subsystem; adding a real weapon changes nothing downstream. That is why this
 * controller stays useful as a debug tool afterwards.
 *
 * The possessed unit carries the weapon and pulls its own trigger; this controller only picks
 * what the weapon fires. 1..9 select a shipped weapon profile, and while one is selected the
 * click is the unit's; 0 clears the profile and returns the click to the hitscan brush.
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
	int32 GetNextSeed() const;

	/** Selects WeaponProfiles[Index - 1] on the pawn's weapon; 0 or out of range returns the click to the hitscan brush. */
	UFUNCTION(BlueprintCallable, Exec, Category = "Sample|Weapon")
	void PaintWeapon(int32 Index);

	/** Console: toggles the floating coverage text over every paintable surface. */
	UFUNCTION(Exec)
	void PaintDebugText();

	/** Console: toggles the coverage cell slabs on every paintable surface. */
	UFUNCTION(Exec)
	void PaintDebugCells();

	/** Console: logs each surface's coverage and the world total. */
	UFUNCTION(Exec)
	void PaintCoverage();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	void OnPaintTriggered();
	void OnSelectWeaponKey(FKey Key);
	void OnContinuousPaintTriggered();
	void OnContinuousPaintReleased();
	void OnCycleTeamTriggered(const FInputActionValue& Value);
	void OnToggleUIFocus();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputMappingContext> PaintMappingContext;

	/**
	 * The same fire action the unit binds (IA_Fire from the gameplay context), so one button is
	 * the trigger everywhere: the unit fires its weapon, and this controller stamps the hitscan
	 * brush only while no weapon profile is selected.
	 */
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

	/** World coverage readout. Defaults to the C++ widget; a UMG subclass restyles it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|UI")
	TSubclassOf<USampleCoverageWidget> CoverageWidgetClass;

	/** The brush this source stamps with: its material and how a hit becomes a splat shape. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Paint")
	TObjectPtr<UPaintBrushProfile> BrushProfile;

	/** Profiles behind the number keys, 1 first. Empty: every profile under WeaponProfileFolder, by name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Weapon")
	TArray<TObjectPtr<UPaintWeaponProfile>> WeaponProfiles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Weapon")
	FString WeaponProfileFolder = TEXT("/Game/Blueprints/Weapons/Profiles");

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

	/** Height fraction a single click deposits. Reaches the max after a few repeat clicks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightPerSplat = 0.35f;

	/** Height fraction each splat of a held stroke deposits; with ContinuousSpacing this sets the fill rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightPerSplatHeld = 0.12f;

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
	bool PaintAtHit(const FHitResult& Hit, const FVector& Direction, float HeightAdd);
	UPaintSubsystem* GetPaintSubsystem() const;

	/** Binds to the pawn's weapon component and carries the selected profile and team over. */
	void BindWeapon(APawn* InPawn);
	void LoadWeaponProfilesFromFolder();
	/** Whether the click currently goes through the weapon rather than the hitscan brush. */
	bool IsWeaponSelected() const;
	void ShowMessage(const FString& Message) const;

	UFUNCTION()
	void OnWeaponFired(int32 Seed);

	/** Creates a widget for the local player and puts it on screen; null for a remote controller or an unset class. */
	template <typename T>
	T* AddLocalWidget(TSubclassOf<T> WidgetClass)
	{
		if (!IsLocalPlayerController() || !WidgetClass)
		{
			return nullptr;
		}
		T* const Widget = CreateWidget<T>(this, WidgetClass);
		if (Widget)
		{
			Widget->AddToViewport();
		}
		return Widget;
	}

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<USampleSeedWidget> SeedWidget;

	UPROPERTY(Transient)
	TObjectPtr<USampleCoverageWidget> CoverageWidget;

	/** The possessed pawn's weapon; re-resolved on every possess, null for a pawn without one. */
	UPROPERTY(Transient)
	TObjectPtr<UPaintWeaponComponent> Weapon;

	/** Index into WeaponProfiles, or INDEX_NONE for the hitscan brush. Survives a respawn. */
	int32 SelectedWeaponIndex = INDEX_NONE;

	FVector StrokeAnchor = FVector::ZeroVector;
	bool bStrokeAnchorValid = false;
	bool bUIFocused = false;
	bool bUseFixedSeed = false;
	int32 NextSeed = 0;
};
