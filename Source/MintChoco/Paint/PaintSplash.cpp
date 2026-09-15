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

	FName SlotName(const TCHAR* Prefix, int32 Index)
	{
		static TMap<FString, TArray<FName>> Cache;
		TArray<FName>& Names = Cache.FindOrAdd(Prefix);
		if (Names.IsEmpty())
		{
			for (int32 Slot = 0; Slot < PaintSplash::MaxDroplets; ++Slot)
			{
				Names.Add(FName(*FString::Printf(TEXT("%s%d"), Prefix, Slot)));
			}
		}
		return Names.IsValidIndex(Index) ? Names[Index] : NAME_None;
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

float PaintSplash::TangentialShare(float NormalSpeed, float TangentialSpeed)
{
	const float Tangential = FMath::Max(TangentialSpeed, 0.0f);
	return Tangential / FMath::Max(Tangential + FMath::Max(NormalSpeed, 0.0f), UE_KINDA_SMALL_NUMBER);
}

void PaintSplash::SplitGroups(const UPaintSplashProfile& Profile, int32 Count, float TangentialShare, int32& OutForward, int32& OutSide, int32& OutBack)
{
	Count = FMath::Max(Count, 0);
	const float ForwardShare = FMath::Lerp(Profile.ForwardShareHeadOn, Profile.ForwardShareGrazing, FMath::Clamp(TangentialShare, 0.0f, 1.0f));
	OutForward = FMath::Clamp(FMath::RoundToInt(Count * FMath::Clamp(ForwardShare, 0.0f, 1.0f)), 0, Count);
	const int32 Rest = Count - OutForward;
	OutBack = FMath::Clamp(FMath::RoundToInt(Rest * FMath::Clamp(Profile.BackShareOfRest, 0.0f, 1.0f)), 0, Rest);
	OutSide = Rest - OutBack;
}

FVector PaintSplash::LaunchOffset(const FVector& Normal, const FVector& Velocity, float Radius, float BallRadius)
{
	const FVector Flat = Velocity - Normal * FVector::DotProduct(Velocity, Normal);
	const FVector Direction = Flat.Size() > 1e-3 ? Flat.GetSafeNormal() : FVector::ZeroVector;
	return Direction * (0.5 * BallRadius) + Normal * (Radius + 1.0);
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
	const float BallRadius = FMath::Max(Input.BallRadius, 0.1f);

	const int32 Count = FMath::Clamp(Profile.DropletCount, 1, MaxDroplets);
	int32 NumForward = 0;
	int32 NumSide = 0;
	int32 NumBack = 0;
	SplitGroups(Profile, Count, TangentialShare(NormalSpeed, TangentialSpeed), NumForward, NumSide, NumBack);

	const struct
	{
		EDropletGroup Group;
		const FPaintSplashDropletGroup& Settings;
		int32 Num;
		float HeadingDeg;
	} Plans[] = {
		{EDropletGroup::Forward, Profile.Forward, NumForward, 0.0f},
		{EDropletGroup::Side, Profile.Side, NumSide, 90.0f},
		{EDropletGroup::Back, Profile.Back, NumBack, 180.0f},
	};
	const float RadiusMin = FMath::Max(Profile.DropletRadiusScale.Min, 0.01f);
	const float RadiusMax = FMath::Max(Profile.DropletRadiusScale.Max, RadiusMin);
	const float RadiusBias = FMath::Max(Profile.DropletRadiusBias, 0.01f);
	for (const auto& Plan : Plans)
	{
		for (int32 Index = 0; Index < Plan.Num; ++Index)
		{
			// Stratified across the group's fan; the side group alternates between the two sides.
			// The draws come in a fixed order per droplet so a knob change moves nothing else.
			const float Fan = Plan.Settings.SpreadDeg * ((static_cast<float>(Index) + Stream.FRand()) / Plan.Num * 2.0f - 1.0f);
			const float Heading = Plan.Group == EDropletGroup::Side && (Index % 2) == 1 ? -Plan.HeadingDeg : Plan.HeadingDeg;
			const float Azimuth = Heading + Fan;
			const float Elevation = FMath::Clamp(Stream.FRandRange(Plan.Settings.ElevationDeg.Min, Plan.Settings.ElevationDeg.Max), 0.0f, 89.0f);
			const float Speed = NormalSpeed * Stream.FRandRange(Plan.Settings.SpeedScale.Min, Plan.Settings.SpeedScale.Max);
			const float RadiusScale = RadiusMin + (RadiusMax - RadiusMin) * FMath::Pow(Stream.FRand(), RadiusBias);

			const FVector InPlane = Forward * FMath::Cos(FMath::DegreesToRadians(Azimuth)) + Side * FMath::Sin(FMath::DegreesToRadians(Azimuth));
			const FVector Direction = Normal * FMath::Cos(FMath::DegreesToRadians(Elevation)) + InPlane * FMath::Sin(FMath::DegreesToRadians(Elevation));
			const FVector Slide = Forward * (Plan.Settings.SlideScale * TangentialSpeed);

			FDroplet& Droplet = OutDroplets.AddDefaulted_GetRef();
			Droplet.Group = Plan.Group;
			Droplet.Radius = RadiusScale * BallRadius;
			Droplet.Velocity = ClampSpeed(Direction * Speed + Slide, Profile.MaxDropletSpeed);
		}
	}

	TArray<float, TInlineAllocator<MaxDroplets>> Radii;
	for (const FDroplet& Droplet : OutDroplets)
	{
		Radii.Add(Droplet.Radius);
	}
	const float Scale = CapVolume(Radii, BallRadius, Profile.VolumeFraction);
	for (FDroplet& Droplet : OutDroplets)
	{
		if (Scale < 1.0f)
		{
			Droplet.Radius *= Scale;
		}
		Droplet.Position = Input.ImpactPoint + LaunchOffset(Normal, Droplet.Velocity, Droplet.Radius, BallRadius);
	}

	// Largest first, so the mark and score caps take prefixes.
	OutDroplets.StableSort([](const FDroplet& A, const FDroplet& B) { return A.Radius > B.Radius; });
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

	const int32 Count = FMath::Min(Droplets.Num(), FMath::Max(Profile.MaxScoreDroplets, 0));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FDroplet& Droplet = Droplets[Index];
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

FVector PaintSplash::BlobScale(const UPaintSplashProfile& Profile, const FSpawnInput& Input, TArrayView<const FDroplet> Droplets, float GravityZ)
{
	const FVector Normal = Input.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Gravity(0.0, 0.0, GravityZ * Profile.GravityScale);
	const double GravityAlong = FVector::DotProduct(Gravity, Normal);

	// A ball's worth of cube at least, so a splash with nothing in the air still has a body; the
	// droplets add their flights, sampled along the parabola until they return to the plane or
	// run out of lifetime, whichever comes first.
	double Reach = Input.BallRadius;
	double Height = Input.BallRadius;
	constexpr int32 Samples = 8;
	for (const FDroplet& Droplet : Droplets)
	{
		const FVector Offset = Droplet.Position - Input.ImpactPoint;
		const double Rise = FVector::DotProduct(Droplet.Velocity, Normal);
		const double Start = FVector::DotProduct(Offset, Normal);
		double Flight = Profile.MaxLifetime;
		if (GravityAlong < -UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			const double Discriminant = Rise * Rise - 2.0 * GravityAlong * Start;
			if (Discriminant >= 0.0)
			{
				Flight = FMath::Min(Flight, (-Rise - FMath::Sqrt(Discriminant)) / GravityAlong);
			}
		}
		for (int32 Sample = 0; Sample <= Samples; ++Sample)
		{
			const double Time = Flight * Sample / Samples;
			const FVector Point = Offset + Droplet.Velocity * Time + Gravity * (0.5 * Time * Time);
			const double Up = FVector::DotProduct(Point, Normal);
			const double Across = (Point - Normal * Up).Size();
			Reach = FMath::Max(Reach, Across + Droplet.Radius);
			Height = FMath::Max(Height, Up + Droplet.Radius);
		}
	}
	Reach = FMath::Min(Reach, static_cast<double>(Profile.MaxTravel)) + Profile.BlobPadding;
	Height += Profile.BlobPadding;
	return FVector(2.0 * Reach, 2.0 * Reach, Height) / 100.0;
}

FName PaintSplashBlob::Drop(int32 Index)
{
	return SlotName(TEXT("Drop"), Index);
}

FName PaintSplashFX::Drop(int32 Index)
{
	return SlotName(TEXT("User.Drop"), Index);
}
