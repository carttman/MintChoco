#pragma once

#include "CoreMinimal.h"

class UPaintSplashProfile;

/**
 * The splash as arithmetic: how a contact turns into droplets and where they come down. Nothing in
 * here touches a world, so the score side, the effect and the tests all share one description.
 * C++ throws the droplets (GenerateDroplets) and hands them to NS_PaintSplash and the blob material
 * one slot each, so the picture, the marks and the phantom landings the score counts are the same
 * droplets: the largest MaxMarkDroplets leave marks, the largest MaxScoreDroplets score.
 */
namespace PaintSplash
{
	/** Droplets one contact can throw. NS_PaintSplash and M_PaintSplashBlob have a slot per droplet. */
	inline constexpr int32 MaxDroplets = 16;

	/** Where a droplet heads relative to the ball's travel: on across it, off to the side, or back the way the ball came. */
	enum class EDropletGroup : uint8
	{
		Forward,
		Side,
		Back,
	};

	struct FSpawnInput
	{
		FVector ImpactPoint = FVector::ZeroVector;
		FVector ImpactNormal = FVector::UpVector;
		/** Velocity of the ball at the contact, cm/s, pointing into the surface. */
		FVector IncidentVelocity = FVector::ZeroVector;
		float BallRadius = 6.0f;
		/** The ball's seed; the splash derives its own stream from it (SplashSeed). */
		int32 Seed = 0;
	};

	struct FDroplet
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Radius = 0.0f;
		EDropletGroup Group = EDropletGroup::Forward;
	};

	/** Where a droplet would come back down on the contact's own plane, and how fast. */
	struct FPhantomLanding
	{
		FVector Point = FVector::ZeroVector;
		float Speed = 0.0f;
	};

	/** The splash's random stream, distinct from the ball's own so its splat is not disturbed. */
	MINTCHOCO_API int32 SplashSeed(int32 BallSeed);

	/** Approach speed (positive when moving into the surface) and the velocity left in the surface plane. */
	MINTCHOCO_API void SplitVelocity(const FVector& Velocity, const FVector& Normal, float& OutNormalSpeed, FVector& OutTangential);

	/** Share of the approach that lies in the surface plane: 0 head-on, 1 grazing. */
	MINTCHOCO_API float TangentialShare(float NormalSpeed, float TangentialSpeed);

	/** How Count droplets split between the groups for a contact with this TangentialShare; the sum is Count and the forward share grows with it. */
	MINTCHOCO_API void SplitGroups(const UPaintSplashProfile& Profile, int32 Count, float TangentialShare, int32& OutForward, int32& OutSide, int32& OutBack);

	/**
	 * Where a droplet starts relative to the contact: half a ball radius along its velocity's
	 * in-plane direction, its radius plus a centimetre off the surface. M_PaintSplashBlob mirrors it.
	 */
	MINTCHOCO_API FVector LaunchOffset(const FVector& Normal, const FVector& Velocity, float Radius, float BallRadius);

	/**
	 * Throws the droplets of one contact, largest first. Empty when the approach is slower than
	 * MinNormalSpeed. Deterministic in the input.
	 */
	MINTCHOCO_API void GenerateDroplets(const UPaintSplashProfile& Profile, const FSpawnInput& Input, TArray<FDroplet>& OutDroplets);

	/**
	 * Shrinks the radii together until their volume fits VolumeFraction of a ball of BallRadius.
	 * Returns the scale applied, 1 when nothing had to change.
	 */
	MINTCHOCO_API float CapVolume(TArrayView<float> Radii, float BallRadius, float VolumeFraction);

	/**
	 * Where the largest MaxScoreDroplets droplets meet the contact's plane again under gravity, drag
	 * ignored: the score's picture of the splash. A droplet that never returns (a wall, a ceiling),
	 * lands after MaxLifetime or farther than MaxTravel is left out. GravityZ is the world's, cm/s^2,
	 * negative down.
	 */
	MINTCHOCO_API void PhantomLandings(const UPaintSplashProfile& Profile, const FSpawnInput& Input, float GravityZ, TArray<FPhantomLanding>& OutLandings);

	/** Smooth-min radius at Age: CohesionRadius at birth, gone after CohesionDecay. */
	MINTCHOCO_API float Cohesion(const UPaintSplashProfile& Profile, float Age);

	/**
	 * Scale of the unit cube the blob material marches in: centred on the contact, standing on its
	 * plane, wide and high enough for every droplet's drag-free flight. In the cube's own 100 cm
	 * units, so it goes straight into Particles.Scale.
	 */
	MINTCHOCO_API FVector BlobScale(const UPaintSplashProfile& Profile, const FSpawnInput& Input, TArrayView<const FDroplet> Droplets, float GravityZ);
}

/** Parameters of M_PaintSplashBlob, set on the dynamic instance UPaintSplashSubsystem builds per splash. */
namespace PaintSplashBlob
{
	inline const FName TeamId(TEXT("TeamId"));
	/** "Drop0".."Drop15": xyz launch velocity cm/s, w radius cm. 0 radius means the slot is empty. */
	MINTCHOCO_API FName Drop(int32 Index);
	/** (gravity cm/s^2 downward, drag 1/s, cohesion radius cm, cohesion decay s). */
	inline const FName Physics(TEXT("Phys"));
	inline const FName BallRadius(TEXT("BallRadius"));
	/** Fraction of a droplet's in-plane speed the puddle rim its strand roots on spreads at. */
	inline const FName PuddleSpread(TEXT("PuddleSpread"));
	/** How far a ray marches past the cube's surface before giving up, cm. */
	inline const FName MarchMax(TEXT("MarchMax"));
}

/**
 * User parameters NS_PaintSplash reads. UPaintSplashSubsystem::ConfigureEffect writes them at the
 * contact; the system declares the same names. Niagara only integrates the droplets it is given,
 * collides them and reports each landing to the handler, which stamps the mark.
 */
namespace PaintSplashFX
{
	/** Droplets to fly, 0 to MaxDroplets: the largest MaxMarkDroplets, 0 when this contact does not splash. */
	inline const FName DropletCount(TEXT("User.DropletCount"));

	/** Object implementing INiagaraParticleCallbackHandler that receives every landing; null leaves the droplets markless. */
	inline const FName LandingHandler(TEXT("User.LandingHandler"));

	inline const FName BallRadius(TEXT("User.BallRadius"));
	inline const FName Seed(TEXT("User.Seed"));

	/** "User.Drop0".."User.Drop15", Vector4: launch velocity cm/s in xyz, radius cm in w. Droplets spawn at the system origin. */
	MINTCHOCO_API FName Drop(int32 Index);

	inline const FName GravityScale(TEXT("User.GravityScale"));
	inline const FName Drag(TEXT("User.Drag"));
	inline const FName MaxLifetime(TEXT("User.MaxLifetime"));
	inline const FName MaxTravel(TEXT("User.MaxTravel"));
	inline const FName CohesionRadius(TEXT("User.CohesionRadius"));
	inline const FName CohesionDecay(TEXT("User.CohesionDecay"));
	/** The blob's dynamic material instance and the scale of the cube it marches in. */
	inline const FName BlobMaterial(TEXT("User.BlobMaterial"));
	inline const FName BlobScale(TEXT("User.BlobScale"));
}
