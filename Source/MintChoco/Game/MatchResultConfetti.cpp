#include "Game/MatchResultConfetti.h"

namespace MatchResultConfetti
{
	/** 태어나는 자리. 화면 위쪽 밖에서 세로로 흩어져 내려와야 한 줄로 쏟아지지 않는다. */
	constexpr float SpawnTop = -0.28f;
	constexpr float SpawnBottom = -0.04f;

	/** 화면 가장자리에서도 반쯤 걸친 스티커가 지나가도록 좌우로 조금 넘겨 뿌린다. */
	constexpr float SpawnSideMargin = 0.06f;

	/** 이만큼 내려가면 수명이 남아 있어도 치운다. 화면 밖에서 도는 장수를 줄인다. */
	constexpr float DespawnBottom = 1.25f;

	float Sample(const FFloatInterval& Interval, FRandomStream& Random)
	{
		return FMath::Lerp(Interval.Min, Interval.Max, Random.GetFraction());
	}
}

FBox2f FMatchResultSpriteSheet::GetCellUV(int32 Cell) const
{
	const int32 ColumnCount = FMath::Max(Columns, 1);
	const int32 RowCount = FMath::Max(Rows, 1);
	const int32 Index = FMath::Abs(Cell) % (ColumnCount * RowCount);

	const FVector2f CellSize(1.0f / ColumnCount, 1.0f / RowCount);
	const FVector2f Corner(Index % ColumnCount * CellSize.X, Index / ColumnCount * CellSize.Y);
	const FVector2f Margin = CellSize * FMath::Clamp(Inset, 0.0f, 0.45f);

	return FBox2f(Corner + Margin, Corner + CellSize - Margin);
}

float FMatchResultConfettiRules::GetOpacity(const FMatchResultSticker& Sticker) const
{
	if (FadeInSeconds > 0.0f && Sticker.Age < FadeInSeconds)
	{
		return FMath::Max(Sticker.Age, 0.0f) / FadeInSeconds;
	}

	const float Remaining = Sticker.Life - Sticker.Age;
	if (FadeOutSeconds > 0.0f && Remaining < FadeOutSeconds)
	{
		return FMath::Max(Remaining, 0.0f) / FadeOutSeconds;
	}
	return 1.0f;
}

void FMatchResultConfetti::Start(const FMatchResultConfettiRules& InRules, int32 InCellCount, int32 Seed)
{
	Rules = InRules;
	CellCount = FMath::Max(InCellCount, 1);
	Random.Initialize(Seed);

	Stickers.Reset();
	Spawned = 0;
	Elapsed = 0.0f;
}

void FMatchResultConfetti::Reset()
{
	Stickers.Reset();
	Spawned = 0;
	Elapsed = 0.0f;
}

int32 FMatchResultConfetti::CountDueBy(float InElapsed) const
{
	if (Rules.SpawnSeconds <= 0.0f)
	{
		return FMath::Max(Rules.Count, 0);
	}
	// 올림이라 첫 프레임에 이미 한 장이 나간다. 뿌리기 시작하는 순간이 비어 보이지 않는다.
	const float Fraction = InElapsed / Rules.SpawnSeconds;
	return FMath::Clamp(FMath::CeilToInt32(Fraction * Rules.Count), 0, FMath::Max(Rules.Count, 0));
}

FMatchResultSticker FMatchResultConfetti::MakeSticker()
{
	using namespace MatchResultConfetti;

	FMatchResultSticker Sticker;
	Sticker.Position = FVector2f(
		Random.FRandRange(-SpawnSideMargin, 1.0f + SpawnSideMargin),
		Random.FRandRange(SpawnTop, SpawnBottom));

	// 내려가는 속도는 0 아래로 내려가지 않게 막는다. 위로 올라가 버리면 영영 돌아오지 않는다.
	Sticker.Velocity = FVector2f(
		Random.FRandRange(-Rules.SideDrift, Rules.SideDrift),
		FMath::Max(Sample(Rules.FallSpeed, Random), 0.01f));

	Sticker.Angle = Random.FRandRange(0.0f, 360.0f);
	Sticker.Spin = Sample(Rules.Spin, Random) * (Random.GetFraction() < 0.5f ? -1.0f : 1.0f);

	Sticker.SwayPhase = Random.FRandRange(0.0f, UE_TWO_PI);
	Sticker.SwayRate = FMath::Max(Sample(Rules.SwayRate, Random), 0.0f);
	Sticker.SwayAmplitude = FMath::Max(Sample(Rules.SwayAmplitude, Random), 0.0f);

	Sticker.Scale = FMath::Max(Sample(Rules.Scale, Random), 0.05f);
	Sticker.Cell = Random.RandRange(0, CellCount - 1);
	Sticker.Life = FMath::Max(Sample(Rules.Life, Random), 0.05f);
	return Sticker;
}

void FMatchResultConfetti::Advance(float DeltaSeconds)
{
	const float DeltaTime = FMath::Max(DeltaSeconds, 0.0f);
	Elapsed += DeltaTime;

	const int32 Due = CountDueBy(Elapsed);
	for (; Spawned < Due; ++Spawned)
	{
		Stickers.Add(MakeSticker());
	}

	for (FMatchResultSticker& Sticker : Stickers)
	{
		Sticker.Age += DeltaTime;
		Sticker.Velocity.Y += Rules.Gravity * DeltaTime;
		Sticker.SwayPhase += Sticker.SwayRate * DeltaTime;

		// 흔들림은 자리가 아니라 속도에 실린다. 그래야 옆으로 새는 속도와 합쳐져 한 번에 그려진다.
		const FVector2f Sway(FMath::Sin(Sticker.SwayPhase) * Sticker.SwayAmplitude, 0.0f);
		Sticker.Position += (Sticker.Velocity + Sway) * DeltaTime;
		Sticker.Angle += Sticker.Spin * DeltaTime;
	}

	Stickers.RemoveAll([](const FMatchResultSticker& Sticker)
	{
		return Sticker.Age >= Sticker.Life || Sticker.Position.Y > MatchResultConfetti::DespawnBottom;
	});
}
