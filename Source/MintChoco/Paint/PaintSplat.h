#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"

#include "PaintSplat.generated.h"

class UMaterialInterface;

/**
 * Size of the paint-id space. Ids 0-3 are player teams, 4-6 are reserved for game
 * elements, and the last id means "nothing painted here" - so painting it erases.
 */
inline constexpr uint8 PaintIdCount = 8;
inline constexpr uint8 PaintIdNone = PaintIdCount - 1;

/** Clear color that fills a paint buffer with PaintIdNone in R, no height and "far" in B. */
inline const FLinearColor PaintIdNoneColor(PaintIdNone / 255.0f, 0.0f, 0.0f);

/**
 * One paint contact, fully resolved: everything a surface needs to draw and score it, and
 * nothing it has to look up. A debug click, a paintball and a mop all produce this same struct
 * through a UPaintBrushProfile; the surface that receives it owns no brush tuning at all.
 *
 * That split is what replication needs. The server builds the splat once, every client draws
 * the identical stamp, and the members are the net-quantized vector types so the struct goes
 * over the wire as-is.
 */
USTRUCT(BlueprintType)
struct FPaintSplat
{
	GENERATED_BODY()

	/** Stamp center in world space, with the incidence shift already applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector_NetQuantize Location = FVector::ZeroVector;

	/** Surface normal at the contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector_NetQuantizeNormal Normal = FVector::UpVector;

	/** Unit stamp U axis in world space: the stretch direction, or a seeded rotation for a round stamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector_NetQuantizeNormal AxisU = FVector::ForwardVector;

	/** Half-extent along the V axis in world cm; along U the stamp spans Radius * Stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	float Radius = 25.0f;

	/** 1 / cos(incidence), clamped. 1 means a head-on hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	float Stretch = 1.0f;

	/** Where the contact sits along U, normalized by the long half-axis. Anchors the stamp's spike field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	float ImpactU = 0.0f;

	/** Id this splat writes into the buffer. PaintIdNone erases back to "unpainted". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 PaintId = 0;

	/** Fraction of the max paint height this contact adds; the buffer accumulates with saturation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightAdd = 0.35f;

	/** Drives shape variation. 16 bits because the stamp shader's hash only keeps that much precision. */
	UPROPERTY(EditAnywhere, Category = "Paint")
	uint16 Seed = 0;

	/** Brush material that stamps this splat into a surface's paint buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	TObjectPtr<UMaterialInterface> BrushMaterial;

	/** Farthest painted point from the center, in world cm. This is the overlap query radius. */
	float GetWorldExtent() const { return Radius * Stretch; }
};

/**
 * The splat expressed in the painted mesh's local frame. The brush shader and the coverage cell
 * grid both consume this one struct, which is what keeps the two layers agreeing on where a
 * splat landed.
 */
struct FPaintLocalStamp
{
	FVector Center = FVector::ZeroVector;

	/** Unit axes of the stamp plane; U is the stretched axis. */
	FVector AxisU = FVector::ForwardVector;
	FVector AxisV = FVector::RightVector;
	FVector Normal = FVector::UpVector;

	/** Half-extent along AxisV in local units; along AxisU the stamp spans Radius * Stretch. */
	float Radius = 0.0f;
	float Stretch = 1.0f;
};
