#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "Math/RandomStream.h"

/**
 * 스프라이트 시트에서 한 칸을 떼어 내는 자. 칸은 왼쪽 위에서 가로로 세어 나간다.
 */
struct MINTCHOCO_API FMatchResultSpriteSheet
{
	int32 Columns = 1;
	int32 Rows = 1;

	/**
	 * 칸 경계에서 안쪽으로 물리는 비율(칸 크기 기준). 이웃 칸의 그림이 새어 들어오는 것을 막는다.
	 *
	 * 시트의 가로세로가 칸 수로 딱 나누어떨어지지 않으면 경계가 반 픽셀씩 밀리므로, 밉맵을 끈
	 * 텍스처라도 한 줄이 새어 들어올 수 있다.
	 */
	float Inset = 0.005f;

	int32 GetCellCount() const { return FMath::Max(Columns, 1) * FMath::Max(Rows, 1); }

	/** Cell 번호의 UV 사각형. 범위를 벗어난 번호는 칸 수로 감는다. */
	FBox2f GetCellUV(int32 Cell) const;
};

/** 떨어지는 스티커 한 장. */
struct MINTCHOCO_API FMatchResultSticker
{
	/** 화면 크기로 나눈 자리. (0,0)이 왼쪽 위, (1,1)이 오른쪽 아래다. */
	FVector2f Position = FVector2f::ZeroVector;

	/** 화면 크기 기준 초당 이동. 세로가 양수면 내려간다. */
	FVector2f Velocity = FVector2f::ZeroVector;

	float Angle = 0.0f;
	float Spin = 0.0f;

	/** 좌우 흔들림. 위상이 돌면서 가로 속도에 Amplitude 만큼 실린다. */
	float SwayPhase = 0.0f;
	float SwayRate = 0.0f;
	float SwayAmplitude = 0.0f;

	/** 기준 크기에 곱하는 배율. */
	float Scale = 1.0f;

	int32 Cell = 0;
	float Age = 0.0f;
	float Life = 1.0f;
};

/**
 * 스티커를 어떻게 뿌리고 어떻게 떨어뜨릴지. 값은 전부 UMatchResultSettings 에서 온다.
 *
 * 길이는 화면의 너비·높이를 1 로 본 비율이다. 해상도가 달라도 같은 그림이 나오게 하려는 것이고,
 * 위젯 크기를 곱하는 것은 화면에 찍을 때뿐이다. 스티커 한 장의 크기(픽셀)만 위젯이 따로 들고 있다.
 */
struct MINTCHOCO_API FMatchResultConfettiRules
{
	/** 한 번의 연출에서 뿌리는 총 장수. */
	int32 Count = 72;

	/** 이 시간에 걸쳐 고르게 나눠 뿌린다. 0 이면 첫 프레임에 다 나간다. */
	float SpawnSeconds = 1.8f;

	/** 한 장이 화면에 머무는 시간(초). */
	FFloatInterval Life = FFloatInterval(2.6f, 4.2f);

	/** 처음 아래로 내려가는 속도(화면 높이/초). */
	FFloatInterval FallSpeed = FFloatInterval(0.16f, 0.38f);

	/** 떨어지면서 붙는 가속(화면 높이/초²). */
	float Gravity = 0.10f;

	/** 옆으로 새는 초기 속도(화면 너비/초). 음수 쪽으로도 같은 폭만큼 흩어진다. */
	float SideDrift = 0.05f;

	/** 좌우로 흔들리는 폭(화면 너비/초). */
	FFloatInterval SwayAmplitude = FFloatInterval(0.03f, 0.10f);

	/** 흔들리는 주기(라디안/초). */
	FFloatInterval SwayRate = FFloatInterval(1.5f, 3.6f);

	/** 도는 속도(도/초). 절반은 반대로 돈다. */
	FFloatInterval Spin = FFloatInterval(25.0f, 130.0f);

	/** 기준 크기에 곱하는 배율. */
	FFloatInterval Scale = FFloatInterval(0.62f, 1.35f);

	float FadeInSeconds = 0.12f;
	float FadeOutSeconds = 0.7f;

	/** 나이에 따른 투명도. 태어날 때 밝아지고 죽기 전에 사라진다. */
	float GetOpacity(const FMatchResultSticker& Sticker) const;
};

/**
 * 떨어지는 스티커 무리. 월드도 위젯도 없이 테스트한다.
 *
 * 한 번 Start 하면 SpawnSeconds 동안 Count 장을 나눠 뿌리고, 각자 수명이 다하면 사라진다.
 * 씨앗이 같으면 같은 그림이 나오므로 값을 바꿔 가며 화면을 견주기 쉽다.
 */
struct MINTCHOCO_API FMatchResultConfetti
{
	/** 뿌리기 시작한다. 떠 있던 것은 지운다. CellCount 는 스프라이트 시트의 칸 수. */
	void Start(const FMatchResultConfettiRules& InRules, int32 InCellCount, int32 Seed);

	/** 한 프레임. 뿌릴 몫을 뿌리고, 살아 있는 것을 움직이고, 죽은 것을 치운다. */
	void Advance(float DeltaSeconds);

	void Reset();

	/** 뿌릴 것도 남지 않고 떠 있는 것도 없다. */
	bool IsDone() const { return Stickers.IsEmpty() && Spawned >= Rules.Count; }

	const TArray<FMatchResultSticker>& GetStickers() const { return Stickers; }
	const FMatchResultConfettiRules& GetRules() const { return Rules; }

private:
	/** 화면 위쪽 밖에 한 장 만든다. */
	FMatchResultSticker MakeSticker();

	/** 이 시각까지 뿌렸어야 할 누적 장수. 프레임이 길어도 정해진 장수가 정해진 시간에 다 나간다. */
	int32 CountDueBy(float InElapsed) const;

	TArray<FMatchResultSticker> Stickers;
	FMatchResultConfettiRules Rules;
	FRandomStream Random = FRandomStream(0x5713C);
	int32 CellCount = 1;
	int32 Spawned = 0;
	float Elapsed = 0.0f;
};
