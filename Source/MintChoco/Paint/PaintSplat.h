#pragma once

#include "CoreMinimal.h"

#include "PaintSplat.generated.h"

/**
 * Size of the paint-id space. Ids 0-3 are player teams, 4-6 are reserved for game
 * elements, and the last id means "nothing painted here" - so painting it erases.
 */
inline constexpr uint8 PaintIdCount = 8;
inline constexpr uint8 PaintIdNone = PaintIdCount - 1;

/**
 * A single paint contact event. Every paint source - a debug click trace, a paintball
 * projectile, a mop dragged along a wall - produces this same struct; only the rate and
 * the values differ. This is the only thing that will be replicated.
 */
USTRUCT(BlueprintType)
struct FPaintSplat
{
	GENERATED_BODY()

	/** World-space contact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector Location = FVector::ZeroVector;

	/** Surface normal at the contact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector Normal = FVector::UpVector;

	/** Incoming velocity, deliberately not normalized: its magnitude is the impact speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector IncidentVelocity = FVector::ZeroVector;

	/** Id this splat writes into the buffer. PaintIdNone erases back to "unpainted". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 PaintId = 0;

	/** How much paint this contact deposits. A mop tick deposits far less than a paintball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	float Volume = 1.0f;

	/** Drives shape variation. Shared across clients so every machine draws the same splat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	int32 Seed = 0;
};

/** The drawing parameters derived from an FPaintSplat and the surface it landed on. */
USTRUCT(BlueprintType)
struct FPaintSplatShape
{
	GENERATED_BODY()

	/** Radius in world cm. ApplySplat converts it into the painted surface's local space. */
	UPROPERTY(BlueprintReadOnly, Category = "Paint")
	float Radius = 0.0f;

	/** 1 / cos(incidence), clamped. 1 means a head-on hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Paint")
	float Stretch = 1.0f;

	/** Surface-tangent direction the splat stretches along. Zero for a head-on hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Paint")
	FVector TangentDirection = FVector::ZeroVector;
};
