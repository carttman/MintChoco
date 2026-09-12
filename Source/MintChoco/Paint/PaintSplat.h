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

/** Ids 0-3 are the player teams: the only ids a speed star can lock, and the width of every per-team shader vector. */
inline constexpr uint8 PaintTeamIdCount = 4;

/**
 * A buffer texel's R byte holds the id in its low three bits and a speed-star generation in the
 * five above: 0 for plain paint, 1-31 for a trail painted under a running star. The generation is
 * what tells the current star's trail (rainbow, locked) from any older one (plain paint again), so
 * a star ending never has to touch the buffer.
 */
inline constexpr uint8 PaintIdBits = 3;
inline constexpr uint8 PaintStarGenMax = 31;

inline constexpr uint8 EncodePaintTexel(uint8 PaintId, uint8 StarGen)
{
	// Unpainted carries no generation, so the buffer's clear value stays a bare PaintIdNone.
	const uint8 Gen = PaintId == PaintIdNone ? 0 : (StarGen > PaintStarGenMax ? PaintStarGenMax : StarGen);
	return static_cast<uint8>(PaintId | (Gen << PaintIdBits));
}

inline constexpr uint8 DecodePaintId(uint8 Texel)
{
	return static_cast<uint8>(Texel & (PaintIdCount - 1));
}

inline constexpr uint8 DecodePaintStarGen(uint8 Texel)
{
	return static_cast<uint8>(Texel >> PaintIdBits);
}

/** The generation after this one. 0 is never a star, so the wrap lands on 1. */
inline constexpr uint8 NextPaintStarGen(uint8 StarGen)
{
	return StarGen >= PaintStarGenMax ? 1 : static_cast<uint8>(StarGen + 1);
}

/** Clear color that fills a paint buffer with PaintIdNone in R, no height and "far" in B. */
inline const FLinearColor PaintIdNoneColor(PaintIdNone / 255.0f, 0.0f, 0.0f);

/**
 * Which generation each team has locked right now: a texel or cell of that team carrying exactly
 * that generation belongs to a running star and cannot be painted over by any other id. 0 means
 * nothing of that team is locked. Packs into one byte per team, so a splat carries it in four.
 */
struct FPaintLockGens
{
	uint8 Gen[PaintTeamIdCount] = {};

	uint8 For(uint8 PaintId) const { return PaintId < PaintTeamIdCount ? Gen[PaintId] : 0; }

	/** Whether paint of this id and generation is locked. */
	bool Locks(uint8 PaintId, uint8 StarGen) const { return StarGen != 0 && StarGen == For(PaintId); }

	uint32 Pack() const
	{
		uint32 Packed = 0;
		for (int32 Id = 0; Id < PaintTeamIdCount; ++Id)
		{
			Packed |= static_cast<uint32>(Gen[Id]) << (Id * 8);
		}
		return Packed;
	}

	static FPaintLockGens Unpack(uint32 Packed)
	{
		FPaintLockGens Locks;
		for (int32 Id = 0; Id < PaintTeamIdCount; ++Id)
		{
			Locks.Gen[Id] = static_cast<uint8>(Packed >> (Id * 8));
		}
		return Locks;
	}
};

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

	/** Speed-star generation written alongside the id; 0 is plain paint. See PaintStarGenMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "31"))
	uint8 StarGen = 0;

	/**
	 * FPaintLockGens, packed, as the authority saw them when it accepted this splat. Carried in the
	 * splat rather than looked up when drawing, so a client replaying the log skips exactly the
	 * texels the server skipped, whatever star state has replicated since.
	 */
	UPROPERTY()
	uint32 LockGens = 0;

	/** Fraction of the max paint height this contact adds; the buffer accumulates with saturation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightAdd = 0.35f;

	/** Drives shape variation. 16 bits because the stamp shader's hash only keeps that much precision. */
	UPROPERTY(EditAnywhere, Category = "Paint")
	uint16 Seed = 0;

	/** Brush material that stamps this splat into a surface's paint buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	TObjectPtr<UMaterialInterface> BrushMaterial;

	/**
	 * True when the contact landed on a direction its surface does not keep, or on a static mesh
	 * with no paint buffer at all. Such a splat is shown as a passing effect and is neither drawn
	 * into a buffer nor scored. The source decides this once, where the hit actor is known, and
	 * every machine follows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	bool bTransient = false;

	/** Farthest painted point from the center, in world cm. This is the overlap query radius. */
	float GetWorldExtent() const { return Radius * Stretch; }
};

/**
 * The splat expressed in the painted mesh's scaled-local frame: the actor's rotation and
 * translation removed, its scale kept, so every length is still a world length. The brush shader
 * and the coverage cell grid both consume this one struct, which is what keeps the two layers
 * agreeing on where a splat landed.
 */
struct FPaintLocalStamp
{
	FVector Center = FVector::ZeroVector;

	/** Unit axes of the stamp plane; U is the stretched axis. */
	FVector AxisU = FVector::ForwardVector;
	FVector AxisV = FVector::RightVector;
	FVector Normal = FVector::UpVector;

	/** Half-extent along AxisV in world cm; along AxisU the stamp spans Radius * Stretch. */
	float Radius = 0.0f;
	float Stretch = 1.0f;
};
