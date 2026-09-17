#pragma once

#include "CoreMinimal.h"

#include "Game/TeamTypes.h"

#include "MatchResult.generated.h"

/**
 * 결과 연출이 지나가는 단계. 한 방향으로만 흐르고 되돌아오지 않는다.
 *
 * Frozen(입력 잠금) → FadingOut(가림막) → Reveal(무대로 바뀐 화면이 밝아짐) →
 * BarEmpty → BarTeaser → BarReal → BarFinish → Characters → Hold → (서버가 로비로 보냄).
 */
UENUM()
enum class EMatchResultPhase : uint8
{
	/** 연출이 돌고 있지 않다. */
	Idle,
	/** 입력만 잠긴 채 경기 화면 그대로. 아이템 효과가 자연스럽게 끝날 시간이다. */
	Frozen,
	/** 가림막이 내려오는 중. */
	FadingOut,
	/** 화면이 덮인 사이에 무대로 갈아치우고 다시 밝아지는 중. */
	Reveal,
	/** 무대가 보이고 게이지는 비어 있다. */
	BarEmpty,
	/** 게이지가 양쪽에서 조금 차오른다. */
	BarTeaser,
	/** 실제 최종 비율로 맞춘다. 격돌은 그 비율이 만들어 내면 저절로 뜬다. */
	BarReal,
	/** 이긴 쪽 게이지를 더 밀고 진 쪽 액체를 탁하게 만든다. */
	BarFinish,
	/** 이긴 쪽은 다가와 춤추고, 진 쪽은 물러나 멈춘다. */
	Characters,
	/** 완성된 그림을 그대로 둔다. */
	Hold,
};

/** Characters 단계에서 한 자리의 캐릭터가 하는 것. */
UENUM()
enum class EMatchResultStep : uint8
{
	/** 제자리에서 계속 서 있는다. 이긴 팀이 정해지지 않았을 때뿐이다. */
	Stay,
	/** 카메라 쪽으로 다가와 춤춘다. */
	Approach,
	/** 카메라에서 물러나 멈추고 회색이 된다. */
	Withdraw,
};

/**
 * 연출이 끝난 경기에서 가져오는 값 전부. 여기 없는 것은 연출이 알 필요가 없다.
 *
 * 커버리지는 바의 왼쪽/오른쪽 자리 기준이다(팀 기준이 아니다). 어느 팀이 어느 자리인지는
 * UPaintBarWidget 의 LeftPaintId / RightPaintId 가 정하고, 무대의 캐릭터도 같은 값을 따른다.
 */
struct MINTCHOCO_API FMatchResultInput
{
	float LeftCoverage = 0.0f;
	float RightCoverage = 0.0f;
	int32 LeftTeam = Teams::Mint;
	int32 RightTeam = Teams::Choco;

	/** 이긴 팀. 무승부가 없으므로 연출이 도는 동안에는 늘 실제 팀이다. */
	int32 WinningTeam = Teams::None;

	/** BarTeaser 단계에서 양쪽에 채워 보이는 비율. 격돌이 먼저 터지지 않게 ClashCoverage 아래여야 한다. */
	float TeaserCoverage = 0.1f;
};

/** 어떤 단계에서 바가 보여야 하는 값. 그대로 FPaintBarPreview 에 옮겨 담는다. */
struct MINTCHOCO_API FMatchResultBarState
{
	float LeftCoverage = 0.0f;
	float RightCoverage = 0.0f;

	/** KO 마무리를 걸 팀. Teams::None 이면 걸지 않는다. */
	int32 ForcedKnockoutTeam = Teams::None;
};

/**
 * 연출의 시계. 월드도 액터도 없이 테스트한다.
 *
 * 각 단계의 길이만 들고 있고, 경과 시간을 단계로 바꾸는 것과 전체 길이를 재는 것이 하는 일의
 * 전부다. 전체 길이는 AGameGameMode 가 로비 복귀를 예약할 때 쓴다 — 두 숫자가 따로 놀면
 * 연출 도중에 맵이 넘어간다.
 */
struct MINTCHOCO_API FMatchResultTimeline
{
	/** 경기가 끝나고 화면이 완전히 덮이기까지. 가림막이 내려오는 시간을 포함한다. */
	float FreezeSeconds = 3.0f;

	/** 가림막 한 번에 걸리는 시간. UScreenFadeSettings::FadeDuration 을 옮겨 담는다. */
	float FadeSeconds = 0.5f;

	float BarEmptySeconds = 2.0f;
	float BarTeaserSeconds = 1.2f;
	float BarRealSeconds = 1.5f;
	float BarFinishSeconds = 0.6f;
	float CharacterSeconds = 0.6f;
	float HoldSeconds = 5.0f;

	/** 이 단계가 몇 초 동안 도는지. Idle 은 0. */
	float GetPhaseSeconds(EMatchResultPhase Phase) const;

	/** 연출이 시작된 뒤 이 단계가 시작되는 시각. */
	float GetPhaseStart(EMatchResultPhase Phase) const;

	/** 경과 시간이 속한 단계. 끝을 지났으면 Hold 로 머문다. */
	EMatchResultPhase GetPhase(float Elapsed) const;

	/** 연출 전체 길이. 서버가 로비 복귀를 이만큼 뒤로 미룬다. */
	float GetTotalSeconds() const;

	/** 다음 단계. Hold 다음은 Idle(끝)이다. */
	static EMatchResultPhase GetNextPhase(EMatchResultPhase Phase);
};

/** 연출의 순수 계산. */
struct MINTCHOCO_API FMatchResultMath
{
	/**
	 * 이 단계에서 바가 보여야 하는 값.
	 *
	 * BarReal 까지는 KO 마무리를 걸지 않는다. 격돌은 따로 트리거하지 않는데, 두 게이지가 맞닿는
	 * 것 자체가 격돌이라 실제 비율이 그렇게 끝난 경기에서만 저절로 뜨기 때문이다. KO 마무리가
	 * 걸리면 격돌은 바 쪽에서 알아서 꺼진다.
	 */
	static FMatchResultBarState MakeBarState(EMatchResultPhase Phase, const FMatchResultInput& Result);

	/** 이 팀의 캐릭터가 Characters 단계에서 할 것. 이긴 팀이 없으면 양쪽 다 Stay. */
	static EMatchResultStep GetStep(int32 SlotTeam, int32 WinningTeam);

	/**
	 * 결과 화면에 세우는 캐릭터의 월드 트랜스폼.
	 *
	 * 자리에서 카메라를 잇는 시선 광선 위로 Offset 만큼 물린다(양수가 카메라 쪽). 화면 위 자리는
	 * 그대로고 크기만 달라진다 - 정면 축과 나란한 선으로 움직이면 원근 때문에 화면 밖으로 벌어진다.
	 *
	 * 얼굴은 카메라의 정면 축이 아니라 카메라 **지점**을 본다. 축과 나란히 세우면 화면 가장자리의
	 * 캐릭터가 바깥으로 돌아선 것처럼 보인다. 머리는 카메라의 위쪽에 최대한 붙여, 카메라가 정수직
	 * 탑다운이든 비스듬하든 화면에서는 늘 똑바로 서서 이쪽을 보는 그림이 된다.
	 *
	 * MeshYawOffset 은 스켈레탈 메시가 자기 공간에서 보는 쪽을 앞으로 돌려 주는 보정이다(이 프로젝트는 -90).
	 */
	static FTransform MakeCharacterTransform(const FTransform& View, const FVector& SlotLocation, float Offset,
		float MeshYawOffset);
};

/**
 * 튀어나오듯 나타나는 모양. 밝아지는 곡선과 부풀었다 가라앉는 크기를 한 벌로 들고 있다.
 *
 * 밝아지는 것은 앞이 빠르고 뒤가 느리다. 눈에 먼저 들고 마무리는 천천히 잦아드는 편이 "나타났다"로
 * 읽히고, 선형이면 같은 시간 동안 밋밋하게 밝아진다. 크기는 1 에서 한 번 부풀었다 정확히 1 로
 * 돌아온다 - 남는 배율이 있으면 화면에 계속 늘어난 그림이 남는다.
 */
struct MINTCHOCO_API FMatchResultPop
{
	/** 다 밝아지는 데 걸리는 시간(초). 0 이하면 즉시 끝난다. */
	float Seconds = 0.5f;

	/** 가장 부풀었을 때의 크기 배율. 1 이면 크기를 건드리지 않는다. */
	float PeakScale = 1.15f;

	/** 가장 부푸는 순간(0~1). 앞쪽일수록 빠르게 부풀고 천천히 내려앉는다. */
	float PeakAt = 0.35f;

	float GetOpacity(float Elapsed) const;
	float GetScale(float Elapsed) const;

	bool IsDone(float Elapsed) const { return Elapsed >= Seconds; }
};
