#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PaintSplashProfile.generated.h"

class UPaintBrushProfile;

/**
 * What a paintball's landing scatters: the crown ring, the jet that jumps back out of the contact
 * and the satellite droplets that fly off it, and the small marks they leave where they come down.
 *
 * One asset feeds both halves of the splash. The picture is each machine's own: C++ throws the
 * droplets (PaintSplash::GenerateDroplets), NS_PaintSplash flies them, and where one lands the
 * droplet brush stamps a mark straight into the paint buffer - no score, no replication. The score
 * is CPU only and deterministic: PaintSplash::PhantomLandings derives approximate landing points
 * from the replicated splat with the same numbers, and those mark the coverage grid without
 * drawing. Same profile, same droplets, so what players see and what they are credited for stay close.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintSplashProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Radius of the mark a droplet leaves when it lands at Speed (cm/s), before PhantomCellRadiusScale. */
	float ComputeMarkRadius(float Speed) const;

	/** Seconds after the contact by which every droplet has landed or died, with a little slack. */
	float GetHoldSeconds() const { return MaxLifetime + 0.5f; }

	/** Satellite droplets besides the jet; PaintSplash::MaxDroplets caps the whole splash at four. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0", ClampMax = "3"))
	int32 SatelliteCount = 3;

	/** Radius of the jet droplet, as a multiple of the ball's radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.05", ForceUnits = "x"))
	float JetRadiusScale = 0.6f;

	/** Radius of a satellite droplet, as a multiple of the ball's radius, varied a little per droplet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.05", ForceUnits = "x"))
	float SatelliteRadiusScale = 0.4f;

	/** The droplets together never hold more than this fraction of the ball's volume; radii shrink to fit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.05", ClampMax = "1", ForceUnits = "x"))
	float VolumeFraction = 0.6f;

	/** A ball approaching the surface slower than this (normal component, cm/s) only splats; a dying lob does not splash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinNormalSpeed = 400.0f;

	/** Jet speed as a fraction of the approach speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "x"))
	float JetSpeedScale = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "x"))
	float SatelliteSpeedMinScale = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "x"))
	float SatelliteSpeedMaxScale = 0.14f;

	/** Fraction of the ball's tangential speed every droplet keeps, so a grazing hit splashes forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "x"))
	float SlideScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float MaxDropletSpeed = 450.0f;

	/** How far the jet leans off the normal toward the direction of travel on a fully grazing hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "80", ForceUnits = "deg"))
	float JetTiltDeg = 15.0f;

	/** Half-angle of the cone around the normal the satellites leave through. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "85", ForceUnits = "deg"))
	float SatelliteConeDeg = 40.0f;

	/** 0 spreads the satellites evenly around the contact, 1 sends them all into the forward half. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float SatelliteTangentBias = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0", ForceUnits = "x"))
	float GravityScale = 1.0f;

	/** Linear drag, per second. The effect applies it; the score heuristic ignores it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0"))
	float Drag = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float MaxLifetime = 1.2f;

	/** A droplet farther than this from the contact vanishes, and the score counts no landing beyond it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "10", ForceUnits = "cm"))
	float MaxTravel = 250.0f;

	/**
	 * Stamps a droplet's mark: its BrushMaterial draws it and ComputeRadius sizes it from the
	 * landing speed. The phantom landings claim cells of the same size. Unset, the droplets fly
	 * but neither mark nor score.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marks")
	TObjectPtr<UPaintBrushProfile> DropletBrush;

	/** Volume of one droplet's mark in the brush's terms, so its radius follows the square root like every splat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marks", meta = (ClampMin = "0", ForceUnits = "x"))
	float DropletSplatVolume = 0.25f;

	/** Fraction of the max paint height one mark adds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marks", meta = (ClampMin = "0", ClampMax = "1"))
	float DropletHeightAdd = 0.35f;

	/** Scales the radius a phantom landing claims in the score grid relative to the mark a droplet draws. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0", ForceUnits = "x"))
	float PhantomCellRadiusScale = 1.0f;

	/** Smooth-min radius holding the droplets and the crown together at birth, in cm; the yogurt knob. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CohesionRadius = 6.0f;

	/** Seconds until the cohesion has decayed to nothing and the droplets have pinched apart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CohesionDecay = 0.25f;

	/** Final radius of the crown ring, as a multiple of the ball's radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ForceUnits = "x"))
	float CrownRadiusScale = 2.5f;

	/** Tube radius of the crown ring at birth, as a multiple of the ball's radius; it thins to nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ForceUnits = "x"))
	float CrownThicknessScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CrownLifetime = 0.35f;
};
