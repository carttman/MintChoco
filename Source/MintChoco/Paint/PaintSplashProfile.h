#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Math/Interval.h"

#include "PaintSplashProfile.generated.h"

class UMaterialInterface;
class UPaintBrushProfile;

/** One heading's share of the splash: how wide its fan is, how steeply and how fast it leaves, how much of the ball's slide it keeps. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintSplashDropletGroup
{
	GENERATED_BODY()

	FPaintSplashDropletGroup() = default;
	FPaintSplashDropletGroup(float InSpreadDeg, const FFloatInterval& InElevationDeg, const FFloatInterval& InSpeedScale, float InSlideScale)
		: SpreadDeg(InSpreadDeg), ElevationDeg(InElevationDeg), SpeedScale(InSpeedScale), SlideScale(InSlideScale)
	{
	}

	/** Half-width of the fan around the group's heading (forward 0, side 90 either way, back 180), degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float SpreadDeg = 45.0f;

	/** Angle off the surface normal; 0 leaves straight up, 90 skims the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FFloatInterval ElevationDeg = FFloatInterval(30.0f, 65.0f);

	/** Launch speed as a fraction of the approach speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FFloatInterval SpeedScale = FFloatInterval(0.08f, 0.16f);

	/** Fraction of the ball's tangential speed the droplet keeps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ForceUnits = "x"))
	float SlideScale = 0.1f;
};

/**
 * What a paintball's landing scatters: droplets thrown mostly on across the ball's travel, some
 * to the sides and a few back the way it came, the small marks they leave where they come down,
 * and the ripple the contact sends through the paint around it.
 *
 * One asset feeds both halves of the splash. The picture is each machine's own: C++ throws the
 * droplets (PaintSplash::GenerateDroplets), the blob material draws every one of them as strands
 * pulling off the puddle, NS_PaintSplash flies the largest MaxMarkDroplets, and where one lands
 * the droplet brush stamps a mark straight into the paint buffer - no score, no replication. The
 * score is CPU only and deterministic: PaintSplash::PhantomLandings derives approximate landing
 * points of the largest MaxScoreDroplets from the replicated splat with the same numbers, and
 * those mark the coverage grid without drawing. Same profile, same droplets, so what players see
 * and what they are credited for stay close; the two caps are where they differ on purpose.
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

	/** Droplets one contact throws; PaintSplash::MaxDroplets caps it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1", ClampMax = "16"))
	int32 DropletCount = 16;

	/** Droplet radius as a multiple of the ball's radius; DropletRadiusBias above 1 favours the small end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FFloatInterval DropletRadiusScale = FFloatInterval(0.12f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.25", ClampMax = "8"))
	float DropletRadiusBias = 2.0f;

	/** The droplets together never hold more than this fraction of the ball's volume; radii shrink to fit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.05", ClampMax = "1", ForceUnits = "x"))
	float VolumeFraction = 0.6f;

	/** A ball approaching the surface slower than this (normal component, cm/s) only splats; a dying lob does not splash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinNormalSpeed = 400.0f;

	/**
	 * Share of the contact's in-plane speed that counts toward how fast the droplets leave. At 0
	 * only the approach along the normal throws them, so a pellet skimming into the floor at full
	 * speed splashes as weakly as one that merely dropped onto it; at 1 a grazing contact throws
	 * as hard as a head-on one. It moves nothing on a head-on hit, which has no in-plane speed.
	 * MinNormalSpeed still gates on the normal alone: a ball rolling along the ground never splashes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float TangentialLaunchShare = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float MaxDropletSpeed = 900.0f;

	/** Share of the droplets that fly on across the ball's travel on a head-on hit ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float ForwardShareHeadOn = 0.4f;

	/** ... and on a fully grazing one; the tangential share of the approach blends between them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float ForwardShareGrazing = 0.75f;

	/** Of the droplets that do not fly forward, the share thrown back the way the ball came; the rest go to the sides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float BackShareOfRest = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch")
	FPaintSplashDropletGroup Forward = FPaintSplashDropletGroup(50.0f, FFloatInterval(35.0f, 70.0f), FFloatInterval(0.10f, 0.20f), 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch")
	FPaintSplashDropletGroup Side = FPaintSplashDropletGroup(35.0f, FFloatInterval(25.0f, 60.0f), FFloatInterval(0.06f, 0.14f), 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch")
	FPaintSplashDropletGroup Back = FPaintSplashDropletGroup(40.0f, FFloatInterval(15.0f, 45.0f), FFloatInterval(0.04f, 0.10f), 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0", ForceUnits = "x"))
	float GravityScale = 1.0f;

	/** Linear drag, per second. The effect applies it; the score heuristic ignores it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0"))
	float Drag = 0.4f;

	/**
	 * A droplet still in the air after this vanishes, and the score counts no landing past it.
	 * The steepest, fastest droplet needs a whole arc back down to the contact's plane, so this
	 * has to outlast that flight or droplets pop out of the air on their way to the floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float MaxLifetime = 1.8f;

	/**
	 * A droplet farther than this from the contact vanishes, and the score counts no landing
	 * beyond it. A grazing contact slides its forward group a long way, so this is also what
	 * clamps the width of the blob's cube.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "10", ForceUnits = "cm"))
	float MaxTravel = 400.0f;

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
	float DropletHeightAdd = 0.2f;

	/** The largest droplets that fly in NS_PaintSplash and leave marks; the blob still draws them all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marks", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxMarkDroplets = 8;

	/** A droplet landing within this multiple of the ball's own splat radius leaves no mark; the splat already covers it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marks", meta = (ClampMin = "0", ForceUnits = "x"))
	float MarkClearanceScale = 1.1f;

	/** Scales the radius a phantom landing claims in the score grid relative to the mark a droplet draws. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0", ForceUnits = "x"))
	float PhantomCellRadiusScale = 1.0f;

	/** The largest droplets whose phantom landings claim cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxScoreDroplets = 4;

	/** Smooth-min radius holding the strands together at birth, in cm; the yogurt knob. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CohesionRadius = 10.0f;

	/** Seconds until the cohesion has decayed to nothing and the strands have pinched off the puddle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CohesionDecay = 0.35f;

	/** Fraction of a droplet's in-plane speed the puddle rim its strand roots on spreads at while the cohesion holds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ClampMax = "1", ForceUnits = "x"))
	float PuddleSpread = 0.35f;

	/**
	 * Draws the droplets as one ray-marched fluid on a cube around the contact (M_PaintSplashBlob).
	 * ConfigureEffect builds a dynamic instance per splash and hands it the droplets, the flight
	 * numbers and the TeamId (PaintSplashBlob names). Unset, only the marks show.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TObjectPtr<UMaterialInterface> BlobMaterial;

	/** Slack the blob's cube keeps around the droplets' flight, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ForceUnits = "cm"))
	float BlobPadding = 4.0f;
};
