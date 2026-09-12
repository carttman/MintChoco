#include "Game/PaintBarClashEffect.h"

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

#include "Game/PaintBar.h"

namespace PaintBarClash
{
	/** 프레임이 길게 멈췄다 돌아와도 입자가 한꺼번에 쏟아지지 않게 하는 상한. */
	constexpr float MaxEmitPerTick = 48.0f;

	/** 불꽃은 튀자마자 공기에 눌려 짧게 날아간다. */
	constexpr float SparkDrag = 1.5f;

	/** 모서리 반지름이 높이의 절반인 둥근 상자. 정사각형으로 그리면 원이다. */
	const FSlateBrush& DiscBrush()
	{
		static const FSlateBrush Brush = []
		{
			FSlateBrush Result;
			Result.DrawAs = ESlateBrushDrawType::RoundedBox;
			Result.TintColor = FSlateColor(FLinearColor::White);
			Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			Result.OutlineSettings.Width = 0.0f;
			return Result;
		}();
		return Brush;
	}

	void AdvanceParticles(TArray<FPaintBarParticle>& Particles, float DeltaTime, float Gravity, float Drag)
	{
		const float Damping = FMath::Max(1.0f - Drag * DeltaTime, 0.0f);
		for (int32 Index = Particles.Num() - 1; Index >= 0; --Index)
		{
			FPaintBarParticle& Particle = Particles[Index];
			Particle.Age += DeltaTime;
			if (Particle.Age >= Particle.Lifetime)
			{
				Particles.RemoveAtSwap(Index, EAllowShrinking::No);
				continue;
			}
			Particle.Velocity.Y += Gravity * DeltaTime;
			Particle.Velocity *= Damping;
			Particle.Position += Particle.Velocity * DeltaTime;
		}
	}

	void DrawDisc(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2f& Center, float Radius, const FLinearColor& Color)
	{
		if (Radius <= 0.0f || Color.A <= 0.0f)
		{
			return;
		}
		const FVector2f Size(Radius * 2.0f, Radius * 2.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)),
			&DiscBrush(), ESlateDrawEffect::None, Color);
	}

	void DrawSegment(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2f& From, const FVector2f& To, float Thickness, const FLinearColor& Color)
	{
		if (Color.A <= 0.0f) return;

		TArray<FVector2f> Points;
		Points.Reserve(2);
		Points.Add(From);
		Points.Add(To);

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, Color, /*bAntialias=*/true, Thickness);
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	/** 누적된 방출량만큼 Emit을 부른다. Strength가 0이면 쌓인 것도 버린다. */
	void EmitAccumulated(float& Carry, float Rate, float DeltaTime, TFunctionRef<void()> Emit)
	{
		if (Rate <= 0.0f)
		{
			Carry = 0.0f;
			return;
		}

		Carry = FMath::Min(Carry + Rate * DeltaTime, MaxEmitPerTick);

		while (Carry >= 1.0f)
		{
			Carry -= 1.0f;
			Emit();
		}
	}
}

// ---------------------------------------------------------------- UPaintBarSparkEffect

void UPaintBarSparkEffect::OnClashBegin(const FPaintBarClashFrame& Frame)
{
	for (int32 Index = 0; Index < BurstCount; ++Index)
	{
		Emit(Frame, 1.25f);
	}
}

void UPaintBarSparkEffect::Tick(const FPaintBarClashFrame& Frame, float DeltaTime)
{
	Time += DeltaTime;

	const float Rate = (EmitRate + FMath::Abs(Frame.ContactVelocity) * EmitPerContactSpeed) * Frame.Strength;
	PaintBarClash::EmitAccumulated(EmitCarry, Rate, DeltaTime, [this, &Frame] { Emit(Frame, 1.0f); });
	PaintBarClash::AdvanceParticles(Sparks, DeltaTime, Gravity, PaintBarClash::SparkDrag);
}

void UPaintBarSparkEffect::Emit(const FPaintBarClashFrame& Frame, float SpeedScale)
{
	if (Sparks.Num() >= MaxSparks) return;

	const float HalfHeight = Frame.LiquidHeight * 0.5f;
	FPaintBarParticle& Spark = Sparks.AddDefaulted_GetRef();
	Spark.Position = Frame.Contact + FVector2f(Random.FRandRange(-1.5f, 1.5f), Random.FRandRange(-HalfHeight, HalfHeight));

	// 사방 방향을 가로로 눌러 위아래로 치우치게 하고, 맞닿는 점이 움직이는 쪽(밀리는 쪽)으로 조금 더 기울인다.
	const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);
	FVector2f Direction(FMath::Cos(Angle) * (1.0f - VerticalBias), FMath::Sin(Angle));
	Direction.X += FMath::Sign(Frame.ContactVelocity) * PushBias * Random.GetFraction();
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2f(0.0f, -1.0f);
	}

	Spark.Velocity = Direction * FMath::Lerp(Speed.Min, Speed.Max, Random.GetFraction()) * SpeedScale;
	Spark.Lifetime = FMath::Max(FMath::Lerp(Lifetime.Min, Lifetime.Max, Random.GetFraction()), 0.01f);
	Spark.Size = Thickness * Random.FRandRange(0.7f, 1.3f);
	Spark.Seed = Random.GetFraction();
}

int32 UPaintBarSparkEffect::Paint(const FPaintBarClashFrame& Frame, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	using namespace PaintBarClash;

	// Slate에는 블룸이 없어서 반투명 원을 겹쳐 빛무리를 만든다. 바깥은 식은 주황, 안쪽은 뜨거운 노랑과 흰 코어.
	if (Frame.Strength > 0.0f && GlowStrength > 0.0f)
	{
		const float Flicker = 0.88f + 0.12f * FMath::Sin(Time * GlowFlicker * UE_TWO_PI) * FMath::Sin(Time * GlowFlicker * 1.7f + 0.6f);
		const float GlowSize = Frame.LiquidHeight * GlowRadius * Flicker;
		const float GlowAlpha = GlowStrength * Frame.Strength;
		DrawDisc(OutDrawElements, LayerId + 1, Geometry, Frame.Contact, GlowSize, WithAlpha(CoolColor, GlowAlpha * 0.22f));
		DrawDisc(OutDrawElements, LayerId + 2, Geometry, Frame.Contact, GlowSize * 0.7f, WithAlpha(CoolColor, GlowAlpha * 0.3f));
		DrawDisc(OutDrawElements, LayerId + 3, Geometry, Frame.Contact, GlowSize * 0.45f, WithAlpha(HotColor, GlowAlpha * 0.55f));
		DrawDisc(OutDrawElements, LayerId + 4, Geometry, Frame.Contact, GlowSize * 0.22f, WithAlpha(CoreColor, GlowAlpha * 0.9f));
	}

	for (const FPaintBarParticle& Spark : Sparks)
	{
		const float Life = Spark.Age / Spark.Lifetime;
		const FLinearColor SparkColor = Life < 0.35f
			? FMath::Lerp(CoreColor, HotColor, Life / 0.35f)
			: FMath::Lerp(HotColor, CoolColor, (Life - 0.35f) / 0.65f);
		const float Twinkle = 0.75f + 0.25f * FMath::Sin((Time + Spark.Seed) * 40.0f);
		const float SparkAlpha = (1.0f - FMath::SmoothStep(0.55f, 1.0f, Life)) * Twinkle;
		const FVector2f Tail = Spark.Position - Spark.Velocity * StreakSeconds;

		DrawSegment(OutDrawElements, LayerId + 5, Geometry, Tail, Spark.Position, Spark.Size * 3.0f, WithAlpha(CoolColor, SparkAlpha * 0.25f));
		DrawSegment(OutDrawElements, LayerId + 6, Geometry, Tail, Spark.Position, Spark.Size, WithAlpha(SparkColor, SparkAlpha));
	}
	return LayerId + 6;
}

void UPaintBarSparkEffect::Reset()
{
	Sparks.Reset();
	EmitCarry = 0.0f;
}

// ---------------------------------------------------------------- UPaintBarSurgeEffect

void UPaintBarSurgeEffect::OnClashBegin(const FPaintBarClashFrame& Frame)
{
	for (int32 Index = 0; Index < BurstCount; ++Index)
	{
		Emit(Frame, 1.2f);
	}
}

void UPaintBarSurgeEffect::Tick(const FPaintBarClashFrame& Frame, float DeltaTime)
{
	Time += DeltaTime;
	SurgeLevel = Frame.Strength * FMath::Lerp(SurgeFloor, 1.0f, FPaintBarMath::Pulse(Time / FMath::Max(SurgePeriod, 0.1f)));

	PaintBarClash::EmitAccumulated(EmitCarry, EmitRate * SurgeLevel, DeltaTime, [this, &Frame] { Emit(Frame, 1.0f); });
	PaintBarClash::AdvanceParticles(Droplets, DeltaTime, Gravity, Drag);
}

void UPaintBarSurgeEffect::Emit(const FPaintBarClashFrame& Frame, float SpeedScale)
{
	if (Droplets.Num() >= MaxDroplets) return;

	const float HalfHeight = Frame.LiquidHeight * 0.5f;
	FPaintBarParticle& Droplet = Droplets.AddDefaulted_GetRef();
	Droplet.bLeftTeam = Random.GetFraction() < 0.5f;

	// 자기 쪽에서 솟아 상대 쪽으로 넘어간다. 대부분 위로 튀고, 일부는 아래로 흘러넘친다.
	const float Across = Droplet.bLeftTeam ? 1.0f : -1.0f;
	const bool bUpward = Random.GetFraction() < 0.75f;
	const float Vertical = bUpward ? -1.0f : 1.0f;
	Droplet.Position = Frame.Contact + FVector2f(-Across * Random.FRandRange(0.0f, 4.0f), Vertical * HalfHeight * Random.FRandRange(0.3f, 1.0f));

	const FVector2f Direction = FVector2f(Across * Random.FRandRange(0.25f, 0.9f), bUpward ? -1.0f : 0.35f).GetSafeNormal();
	Droplet.Velocity = Direction * FMath::Lerp(Speed.Min, Speed.Max, Random.GetFraction()) * SpeedScale;
	Droplet.Lifetime = FMath::Max(FMath::Lerp(Lifetime.Min, Lifetime.Max, Random.GetFraction()), 0.01f);
	Droplet.Size = FMath::Lerp(DropletRadius.Min, DropletRadius.Max, Random.GetFraction());
	Droplet.Seed = Random.GetFraction();
}

int32 UPaintBarSurgeEffect::Paint(const FPaintBarClashFrame& Frame, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	using namespace PaintBarClash;

	for (const FPaintBarParticle& Droplet : Droplets)
	{
		// 걸쭉한 방울이라 끝까지 불투명하게 버티다가 작아지며 사라진다.
		const float Life = Droplet.Age / Droplet.Lifetime;
		const float Shrink = 1.0f - FMath::SmoothStep(0.7f, 1.0f, Life);
		const float Size = Droplet.Size * Shrink;
		const FLinearColor& TeamColor = Droplet.bLeftTeam ? Frame.LeftColor : Frame.RightColor;

		DrawDisc(OutDrawElements, LayerId + 1, Geometry, Droplet.Position, Size, TeamColor);
		DrawDisc(OutDrawElements, LayerId + 2, Geometry, Droplet.Position + FVector2f(-0.3f, -0.35f) * Size, Size * 0.35f,
			FLinearColor(1.0f, 1.0f, 1.0f, HighlightStrength * Shrink));
	}
	return LayerId + 2;
}

float UPaintBarSurgeEffect::GetBoundaryTurbulence() const
{
	return Turbulence * SurgeLevel;
}

float UPaintBarSurgeEffect::GetFoamWidth() const
{
	return SurgeLevel > 0.0f ? FoamWidth : 0.0f;
}

void UPaintBarSurgeEffect::Reset()
{
	Droplets.Reset();

	EmitCarry = 0.0f;
	SurgeLevel = 0.0f;
}
