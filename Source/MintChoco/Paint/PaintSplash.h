#pragma once

#include "CoreMinimal.h"

class UPaintSplashProfile;

/**
 * The splash as arithmetic: how a contact turns into droplets and where they come down. Nothing in
 * here touches a world, so the score side, the effect and the tests all share one description.
 * C++ throws the droplets (GenerateDroplets) and hands them to NS_PaintSplash one slot each, so the
 * picture, the marks and the phantom landings the score counts are the same four droplets.
 */
namespace PaintSplash
{
	/** Droplets one contact can throw: the jet plus up to three satellites. NS_PaintSplash has a User slot per droplet. */
	inline constexpr int32 MaxDroplets = 4;

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
		bool bJet = false;
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

	/**
	 * Throws the droplets of one contact: index 0 is the jet, the rest satellites. Empty when the
	 * approach is slower than MinNormalSpeed. Deterministic in the input.
	 */
	MINTCHOCO_API void GenerateDroplets(const UPaintSplashProfile& Profile, const FSpawnInput& Input, TArray<FDroplet>& OutDroplets);

	/**
	 * Shrinks the radii together until their volume fits VolumeFraction of a ball of BallRadius.
	 * Returns the scale applied, 1 when nothing had to change.
	 */
	MINTCHOCO_API float CapVolume(TArrayView<float> Radii, float BallRadius, float VolumeFraction);

	/**
	 * Where each droplet meets the contact's plane again under gravity, drag ignored: the score's
	 * picture of the splash. A droplet that never returns (a wall, a ceiling), lands after
	 * MaxLifetime or farther than MaxTravel is left out. GravityZ is the world's, cm/s^2, negative down.
	 */
	MINTCHOCO_API void PhantomLandings(const UPaintSplashProfile& Profile, const FSpawnInput& Input, float GravityZ, TArray<FPhantomLanding>& OutLandings);

	/** Smooth-min radius at Age: CohesionRadius at birth, gone after CohesionDecay. */
	MINTCHOCO_API float Cohesion(const UPaintSplashProfile& Profile, float Age);

	/** The crown ring at Age: its radius grows from half the ball to CrownRadiusScale while the tube thins and fades to 0 at CrownLifetime. */
	MINTCHOCO_API void CrownAt(const UPaintSplashProfile& Profile, float BallRadius, float Age, float& OutRadius, float& OutTube, float& OutFade);
}

/**
 * User parameters NS_PaintSplash reads. UPaintSplashSubsystem::ConfigureEffect writes them at the
 * contact; the system declares the same names. Niagara only integrates the droplets it is given,
 * collides them and reports each landing to the handler, which stamps the mark.
 */
namespace PaintSplashFX
{
	/** Droplets to spawn, 0 to MaxDroplets; 0 when this contact does not splash. */
	inline const FName DropletCount(TEXT("User.DropletCount"));

	/** Object implementing INiagaraParticleCallbackHandler that receives every landing; null leaves the droplets markless. */
	inline const FName LandingHandler(TEXT("User.LandingHandler"));

	inline const FName BallRadius(TEXT("User.BallRadius"));
	inline const FName Seed(TEXT("User.Seed"));

	/** Per droplet slot, 0 the jet: launch position relative to the contact (cm), launch velocity (cm/s), radius (cm). */
	inline const FName DropOffset[PaintSplash::MaxDroplets] = {
		FName(TEXT("User.Drop0Offset")), FName(TEXT("User.Drop1Offset")), FName(TEXT("User.Drop2Offset")), FName(TEXT("User.Drop3Offset"))};
	inline const FName DropVelocity[PaintSplash::MaxDroplets] = {
		FName(TEXT("User.Drop0Velocity")), FName(TEXT("User.Drop1Velocity")), FName(TEXT("User.Drop2Velocity")), FName(TEXT("User.Drop3Velocity"))};
	inline const FName DropRadius[PaintSplash::MaxDroplets] = {
		FName(TEXT("User.Drop0Radius")), FName(TEXT("User.Drop1Radius")), FName(TEXT("User.Drop2Radius")), FName(TEXT("User.Drop3Radius"))};

	inline const FName GravityScale(TEXT("User.GravityScale"));
	inline const FName Drag(TEXT("User.Drag"));
	inline const FName MaxLifetime(TEXT("User.MaxLifetime"));
	inline const FName MaxTravel(TEXT("User.MaxTravel"));
	inline const FName CohesionRadius(TEXT("User.CohesionRadius"));
	inline const FName CohesionDecay(TEXT("User.CohesionDecay"));
	inline const FName CrownRadiusScale(TEXT("User.CrownRadiusScale"));
	inline const FName CrownThicknessScale(TEXT("User.CrownThicknessScale"));
	inline const FName CrownLifetime(TEXT("User.CrownLifetime"));
}
