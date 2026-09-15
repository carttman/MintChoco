#include "Paint/PaintSplash.h"

#include "Paint/PaintSplashProfile.h"

namespace
{
	/** A unit vector in the surface plane for a head-on hit, spun by the seed so the pattern still varies. */
	FVector SeededTangent(const FVector& Normal, const FRandomStream& Stream)
	{
		const FVector Reference = FMath::Abs(Normal.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		const FVector Base = FVector::CrossProduct(Normal, Reference).GetSafeNormal();
		return Base.RotateAngleAxis(Stream.FRand() * 360.0f, Normal);
	}

	FVector ClampSpeed(const FVector& Velocity, float MaxSpeed)
	{
		const double Speed = Velocity.Size();
		return Speed > MaxSpeed && Speed > UE_DOUBLE_KINDA_SMALL_NUMBER ? Velocity * (MaxSpeed / Speed) : Velocity;
	}
}

int32 PaintSplash::SplashSeed(int32 BallSeed)
{
	return static_cast<int32>(HashCombineFast(static_cast<uint32>(BallSeed), 0x53504C48u));
}

void PaintSplash::SplitVelocity(const FVector& Velocity, const FVector& Normal, float& OutNormalSpeed, FVector& OutTangential)
{
	const double Along = FVector::DotProduct(Velocity, Normal);
	OutNormalSpeed = static_cast<float>(-Along);
	OutTangential = Velocity - Normal * Along;
}

void PaintSplash::GenerateDroplets(const UPaintSplashProfile& Profile, const FSpawnInput& Input, TArray<FDroplet>& OutDroplets)
{
	OutDroplets.Reset();

	const FVector Normal = Input.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	float NormalSpeed = 0.0f;
	FVector Tangential;
	SplitVelocity(Input.IncidentVelocity, Normal, NormalSpeed, Tangential);
	if (NormalSpeed < Profile.MinNormalSpeed)
	{
		return;
	}

	const FRandomStream Stream(SplashSeed(Input.Seed));
	const float TangentialSpeed = static_cast<float>(Tangential.Size());
	const FVector Forward = TangentialSpeed > 1.0f ? Tangential / TangentialSpeed : SeededTangent(Normal, Stream);
	const FVector Side = FVector::CrossProduct(Normal, Forward).GetSafeNormal();
	const FVector Slide = Forward * (Profile.SlideScale * TangentialSpeed);
	const float BallRadius = FMath::Max(Input.BallRadius, 0.1f);

	// The jet leans toward the direction of travel by the share the tangential speed has in the whole.
	{
		const float Tilt = Profile.JetTiltDeg * TangentialSpeed / FMath::Max(TangentialSpeed + NormalSpeed, UE_KINDA_SMALL_NUMBER);
		const FVector Direction = Normal.RotateAngleAxis(Tilt, Side);
		FDroplet& Jet = OutDroplets.AddDefaulted_GetRef();
		Jet.bJet = true;
		Jet.Radius = Profile.JetRadiusScale * BallRadius;
		Jet.Velocity = ClampSpeed(Direction * (Profile.JetSpeedScale * NormalSpeed) + Slide, Profile.MaxDropletSpeed);
		Jet.Position = Input.ImpactPoint + Normal * (Jet.Radius + 1.0f);
	}

	const int32 Count = FMath::Clamp(Profile.SatelliteCount, 0, MaxDroplets - 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Stratified around the circle, then pulled toward the forward half by the bias.
		const float Around = (static_cast<float>(Index) + Stream.FRand()) * (360.0f / FMath::Max(Count, 1));
		const float Ahead = Stream.FRandRange(-90.0f, 90.0f);
		const float Azimuth = FMath::Lerp(Around, Ahead, FMath::Clamp(Profile.SatelliteTangentBias, 0.0f, 1.0f));
		const float Elevation = Stream.FRandRange(0.35f, 1.0f) * Profile.SatelliteConeDeg;
		const FVector InPlane = Forward * FMath::Cos(FMath::DegreesToRadians(Azimuth)) + Side * FMath::Sin(FMath::DegreesToRadians(Azimuth));
		const FVector Direction = Normal * FMath::Cos(FMath::DegreesToRadians(Elevation)) + InPlane * FMath::Sin(FMath::DegreesToRadians(Elevation));
		const float Speed = NormalSpeed * Stream.FRandRange(Profile.SatelliteSpeedMinScale, Profile.SatelliteSpeedMaxScale);

		FDroplet& Satellite = OutDroplets.AddDefaulted_GetRef();
		Satellite.Radius = Profile.SatelliteRadiusScale * BallRadius * Stream.FRandRange(0.8f, 1.2f);
		Satellite.Velocity = ClampSpeed(Direction * Speed + Slide, Profile.MaxDropletSpeed);
		Satellite.Position = Input.ImpactPoint + Normal * (Satellite.Radius + 1.0f) + InPlane * (0.5f * BallRadius);
	}

	TArray<float, TInlineAllocator<MaxDroplets>> Radii;
	for (const FDroplet& Droplet : OutDroplets)
	{
		Radii.Add(Droplet.Radius);
	}
	const float Scale = CapVolume(Radii, BallRadius, Profile.VolumeFraction);
	if (Scale < 1.0f)
	{
		for (FDroplet& Droplet : OutDroplets)
		{
			Droplet.Radius *= Scale;
		}
	}
}

float PaintSplash::CapVolume(TArrayView<float> Radii, float BallRadius, float VolumeFraction)
{
	// Spheres all share the 4/3 pi, so the cap is a ratio of cubes.
	double Total = 0.0;
	for (const float Radius : Radii)
	{
		Total += FMath::Cube(static_cast<double>(FMath::Max(Radius, 0.0f)));
	}
	const double Allowed = FMath::Cube(static_cast<double>(FMath::Max(BallRadius, 0.0f))) * FMath::Max(VolumeFraction, 0.0f);
	if (Total <= Allowed || Total <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 1.0f;
	}
	const float Scale = static_cast<float>(FMath::Pow(Allowed / Total, 1.0 / 3.0));
	for (float& Radius : Radii)
	{
		Radius *= Scale;
	}
	return Scale;
}

void PaintSplash::PhantomLandings(const UPaintSplashProfile& Profile, const FSpawnInput& Input, float GravityZ, TArray<FPhantomLanding>& OutLandings)
{
	OutLandings.Reset();

	TArray<FDroplet> Droplets;
	GenerateDroplets(Profile, Input, Droplets);
	if (Droplets.IsEmpty())
	{
		return;
	}

	const FVector Normal = Input.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Gravity(0.0, 0.0, GravityZ * Profile.GravityScale);
	// Gravity's pull back toward the plane. Zero or positive means the droplet never returns to
	// this surface (a wall, a ceiling); it falls somewhere the score does not guess at.
	const double GravityAlong = FVector::DotProduct(Gravity, Normal);
	if (GravityAlong >= -UE_DOUBLE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const FDroplet& Droplet : Droplets)
	{
		// Height above the plane: h(t) = h0 + v t + a t^2 / 2 with a < 0. The later root is the landing.
		const double Height = FVector::DotProduct(Droplet.Position - Input.ImpactPoint, Normal);
		const double Rise = FVector::DotProduct(Droplet.Velocity, Normal);
		const double Discriminant = Rise * Rise - 2.0 * GravityAlong * Height;
		if (Discriminant < 0.0)
		{
			continue;
		}
		const double Time = (-Rise - FMath::Sqrt(Discriminant)) / GravityAlong;
		if (Time <= 0.0 || Time > Profile.MaxLifetime)
		{
			continue;
		}

		const FVector Point = Droplet.Position + Droplet.Velocity * Time + Gravity * (0.5 * Time * Time);
		if (FVector::Dist(Point, Input.ImpactPoint) > Profile.MaxTravel)
		{
			continue;
		}
		FPhantomLanding& Landing = OutLandings.AddDefaulted_GetRef();
		Landing.Point = Point;
		Landing.Speed = static_cast<float>((Droplet.Velocity + Gravity * Time).Size());
	}
}

float PaintSplash::Cohesion(const UPaintSplashProfile& Profile, float Age)
{
	return Profile.CohesionRadius * FMath::Max(0.0f, 1.0f - Age / FMath::Max(Profile.CohesionDecay, UE_KINDA_SMALL_NUMBER));
}

void PaintSplash::CrownAt(const UPaintSplashProfile& Profile, float BallRadius, float Age, float& OutRadius, float& OutTube, float& OutFade)
{
	const float Alpha = FMath::Clamp(Age / FMath::Max(Profile.CrownLifetime, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	OutRadius = FMath::Lerp(0.5f * BallRadius, Profile.CrownRadiusScale * BallRadius, FMath::SmoothStep(0.0f, 1.0f, Alpha));
	OutTube = Profile.CrownThicknessScale * BallRadius * (1.0f - Alpha);
	OutFade = 1.0f - Alpha;
}
