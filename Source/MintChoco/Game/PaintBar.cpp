#include "Game/PaintBar.h"

namespace PaintBar
{
	/** 게이지 길이 비교의 여유. 0.7과 1 - 0.3이 부동소수로 어긋나도 선에 닿은 것으로 본다. */
	constexpr float LineTolerance = 1.0e-5f;
}

// ---------------------------------------------------------------- FPaintBarFill

bool FPaintBarFill::IsClashing() const
{
	return Left > 0.0f && Right > 0.0f && Left + Right >= 1.0f - UE_KINDA_SMALL_NUMBER;
}

// ---------------------------------------------------------------- FPaintKoClock

void FPaintKoClock::Advance(bool bPastLine, float DeltaTime, float HoldSeconds)
{
	if (!bPastLine)
	{
		Held = 0.0f;
		bKnockedOut = false;
		return;
	}

	const float Hold = FMath::Max(HoldSeconds, 0.0f);
	Held = FMath::Min(Held + FMath::Max(DeltaTime, 0.0f), Hold);
	bKnockedOut = Held >= Hold;
}

float FPaintKoClock::GetProgress(float HoldSeconds) const
{
	if (HoldSeconds <= 0.0f)
	{
		return bKnockedOut ? 1.0f : 0.0f;
	}
	return FMath::Clamp(Held / HoldSeconds, 0.0f, 1.0f);
}

int32 FPaintKoClock::GetSecondsLeft(float HoldSeconds) const
{
	return FMath::CeilToInt(FMath::Max(HoldSeconds - Held, 0.0f));
}

// ---------------------------------------------------------------- FPaintBarMath

FPaintBarFill FPaintBarMath::ComputeFill(float LeftCoverage, float RightCoverage, float ClashCoverage)
{
	const float Left = FMath::Max(LeftCoverage, 0.0f);
	const float Right = FMath::Max(RightCoverage, 0.0f);
	const float Denominator = FMath::Max(Left + Right, ClashCoverage);
	if (Denominator <= UE_SMALL_NUMBER)
	{
		return FPaintBarFill();
	}

	FPaintBarFill Fill;
	Fill.Left = Left / Denominator;
	Fill.Right = Right / Denominator;
	return Fill;
}

bool FPaintBarMath::IsPastKoLine(float OpponentFill, float KoLine)
{
	return OpponentFill + PaintBar::LineTolerance >= 1.0f - KoLine;
}

bool FPaintBarMath::IsInDanger(float OpponentFill, const FPaintBarRules& Rules)
{
	return OpponentFill + PaintBar::LineTolerance >= 1.0f - Rules.KoLine - Rules.DangerMargin;
}

FPaintBarFill FPaintBarMath::Exaggerate(const FPaintBarFill& Fill, float IntoLeft, float IntoRight)
{
	FPaintBarFill Result = Fill;

	Result.Right = FMath::Clamp(Result.Right + FMath::Max(IntoLeft, 0.0f), 0.0f, 1.0f);
	Result.Left = FMath::Clamp(Result.Left, 0.0f, 1.0f - Result.Right);

	Result.Left = FMath::Clamp(Result.Left + FMath::Max(IntoRight, 0.0f), 0.0f, 1.0f);
	Result.Right = FMath::Clamp(Result.Right, 0.0f, 1.0f - Result.Left);
	return Result;
}

float FPaintBarMath::Pulse(float Phase)
{
	return 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * Phase);
}

FVector2f FPaintBarMath::DemoCoverage(float Time, float Period)
{
	struct FKey
	{
		float At;
		float Left;
		float Right;
	};

	// 0.55~0.80 구간에서 오른쪽 게이지가 0.78로 판정선(0.7)을 넘긴 채 머문다.
	static constexpr FKey Keys[] = {
		{0.00f, 0.00f, 0.00f},
		{0.20f, 0.22f, 0.26f},
		{0.32f, 0.28f, 0.34f},
		{0.45f, 0.20f, 0.42f},
		{0.55f, 0.14f, 0.50f},
		{0.80f, 0.14f, 0.50f},
		{0.90f, 0.30f, 0.34f},
		{1.00f, 0.00f, 0.00f},
	};

	const float SafePeriod = FMath::Max(Period, UE_KINDA_SMALL_NUMBER);
	const float U = FMath::Fmod(FMath::Max(Time, 0.0f), SafePeriod) / SafePeriod;
	for (int32 Index = 1; Index < static_cast<int32>(UE_ARRAY_COUNT(Keys)); ++Index)
	{
		const FKey& From = Keys[Index - 1];
		const FKey& To = Keys[Index];
		if (U <= To.At)
		{
			const float Alpha = FMath::SmoothStep(From.At, To.At, U);
			return FVector2f(FMath::Lerp(From.Left, To.Left, Alpha), FMath::Lerp(From.Right, To.Right, Alpha));
		}
	}
	return FVector2f::ZeroVector;
}
