#pragma once

#include "CoreMinimal.h"

#include "PaintBar.generated.h"

/**
 * 칠한 비율 바의 규칙. 경기에서는 AGameGameState(ClashCoverage, KnockoutLine, KnockoutHoldSeconds)가 같은
 * 규칙으로 서버에서 KO 를 판정하고, 바는 그 값을 읽어 그린다. 여기 값은 GameState 가 없는 곳(샘플 맵,
 * 위젯 디자이너 미리보기)에서만 쓰는 대체값이다. DangerMargin 만 순수 표시 규칙이다.
 */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintBarRules
{
	GENERATED_BODY()

	/**
	 * 두 팀이 칠한 합이 칠할 수 있는 전체 면적의 이 비율에 닿으면 두 게이지가 가운데에서 만난다. 그 전에는 양 끝에서 차오른다.
	 * 경기에서는 AGameGameState::GetClashCoverage 가 대신한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0.01", ClampMax = "1"))
	float ClashCoverage = 0.6f;

	/**
	 * 양 끝에서 KO 판정선까지의 거리. 게이지 길이에 대한 비율이라 선은 움직이지 않는다.
	 * 경기에서는 AGameGameState::GetKnockoutLine 이 대신한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0.01", ClampMax = "0.45"))
	float KoLine = 0.3f;

	/** 상대 게이지가 판정선에서 이 거리 안으로 들어오면 위험 점멸을 시작한다. 게이지 길이에 대한 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0", ClampMax = "0.45"))
	float DangerMargin = 0.05f;

	/** 경기 밖에서 쓰는 KO 유지 시간. 경기에서는 AGameGameState::GetKnockoutHoldSeconds 가 대신한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar|Preview", meta = (ClampMin = "0", ForceUnits = "s"))
	float PreviewKoHoldSeconds = 5.0f;
};

/** 실제 커버리지 대신 바에 넣는 값. 위젯 디자이너와 샘플 맵이 쓴다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintBarPreview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar")
	bool bEnabled = false;

	/** 왼쪽 팀이 칠한 비율. 칠할 수 있는 전체 면적 기준이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnabled && !bLoopDemo"))
	float LeftCoverage = 0.25f;

	/** 오른쪽 팀이 칠한 비율. 칠할 수 있는 전체 면적 기준이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnabled && !bLoopDemo"))
	float RightCoverage = 0.3f;

	/** 빈 바 → 격돌 → 위험 → 선 넘김 → KO → 회복을 반복한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (EditCondition = "bEnabled"))
	bool bLoopDemo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "4", ForceUnits = "s", EditCondition = "bEnabled && bLoopDemo"))
	float DemoPeriod = 20.0f;
};

/** 게이지가 채운 길이. 각자 자기 끝에서 잰 비율이고 둘의 합은 1을 넘지 않는다. */
struct MINTCHOCO_API FPaintBarFill
{
	float Left = 0.0f;
	float Right = 0.0f;

	/** 두 앞머리가 맞닿았는지. 한쪽이 비어 있으면 맞닿을 상대가 없다. */
	bool IsClashing() const;
};

/** 한 팀의 KO 시계. 판정선을 넘긴 채로 버틴 시간을 센다. */
struct MINTCHOCO_API FPaintKoClock
{
	float Held = 0.0f;
	bool bKnockedOut = false;

	/** 선 밖으로 나오면 처음부터 다시 센다. */
	void Advance(bool bPastLine, float DeltaTime, float HoldSeconds);

	/** 링 게이지가 찬 정도. 0..1. */
	float GetProgress(float HoldSeconds) const;

	/** 링 가운데 숫자. 올림이라 2.1초가 남으면 3이다. */
	int32 GetSecondsLeft(float HoldSeconds) const;

	bool IsCounting() const { return Held > 0.0f || bKnockedOut; }
};

/** 바 표시의 순수 계산. 월드 없이 테스트한다. */
struct MINTCHOCO_API FPaintBarMath
{
	/**
	 * 칠한 비율을 게이지 길이로 바꾼다. 합이 ClashCoverage보다 작으면 ClashCoverage로 나눠 양 끝에서 차오르고,
	 * 넘으면 합으로 나눠 두 팀의 비율대로 나뉜다. 두 식은 합이 ClashCoverage인 순간 같은 값이라 끊기지 않는다.
	 */
	static FPaintBarFill ComputeFill(float LeftCoverage, float RightCoverage, float ClashCoverage);

	/**
	 * 상대 게이지가 내 쪽 판정선에 닿았는지. 게이지는 ComputeFill 의 값이라 격돌 전이면 ClashCoverage 로,
	 * 뒤면 두 팀의 합으로 나눈 몫이다. AGameGameState 가 서버에서 하는 것과 같은 비교다.
	 */
	static bool IsPastKoLine(float OpponentFill, float KoLine);

	/** 상대 게이지가 내 쪽 판정선 DangerMargin 안에 들어왔는지. 선을 넘긴 뒤에도 참이다. */
	static bool IsInDanger(float OpponentFill, float KoLine, float DangerMargin);

	/** KO 연출용. 오른쪽 게이지를 IntoLeft만큼 왼쪽으로, 왼쪽 게이지를 IntoRight만큼 오른쪽으로 더 밀어 넣는다. 합은 1을 넘지 않는다. */
	static FPaintBarFill Exaggerate(const FPaintBarFill& Fill, float IntoLeft, float IntoRight);

	/** 서서히 켜졌다 서서히 꺼지는 점멸 한 주기. Phase 0에서 0, 0.5에서 1. */
	static float Pulse(float Phase);

	/**
	 * 미리보기용 커버리지 곡선. X가 왼쪽, Y가 오른쪽이다. 한 주기 동안 빈 바 → 격돌 → 오른쪽 우세로 위험 →
	 * 선 넘김을 KoHoldSeconds보다 길게 유지 → 회복을 지난다.
	 */
	static FVector2f DemoCoverage(float Time, float Period);
};
