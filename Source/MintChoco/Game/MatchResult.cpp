#include "Game/MatchResult.h"

float FMatchResultTimeline::GetPhaseSeconds(EMatchResultPhase Phase) const
{
	switch (Phase)
	{
	// 가림막이 내려오는 시간도 "3초 대기" 안에 든다. 경기가 끝나고 정확히 FreezeSeconds 만에
	// 화면이 완전히 덮이도록 앞쪽을 그만큼 줄인다.
	case EMatchResultPhase::Frozen:     return FMath::Max(FreezeSeconds - FadeSeconds, 0.0f);
	case EMatchResultPhase::FadingOut:  return FMath::Min(FreezeSeconds, FadeSeconds);
	case EMatchResultPhase::Reveal:     return FadeSeconds;
	case EMatchResultPhase::BarEmpty:   return BarEmptySeconds;
	case EMatchResultPhase::BarTeaser:  return BarTeaserSeconds;
	case EMatchResultPhase::BarReal:    return BarRealSeconds;
	case EMatchResultPhase::BarFinish:  return BarFinishSeconds;
	case EMatchResultPhase::Characters: return CharacterSeconds;
	case EMatchResultPhase::Hold:       return HoldSeconds;
	default:                            return 0.0f;
	}
}

float FMatchResultTimeline::GetPhaseStart(EMatchResultPhase Phase) const
{
	// Idle 을 넘기면 마지막 단계까지 다 더해 연출의 끝을 돌려준다.
	float Start = 0.0f;
	for (EMatchResultPhase Step = EMatchResultPhase::Frozen; Step != Phase && Step != EMatchResultPhase::Idle; Step = GetNextPhase(Step))
	{
		Start += GetPhaseSeconds(Step);
	}
	return Start;
}

EMatchResultPhase FMatchResultTimeline::GetPhase(float Elapsed) const
{
	float Start = 0.0f;
	for (EMatchResultPhase Step = EMatchResultPhase::Frozen; Step != EMatchResultPhase::Idle; Step = GetNextPhase(Step))
	{
		const float End = Start + GetPhaseSeconds(Step);
		if (Elapsed < End)
		{
			return Step;
		}
		Start = End;
	}
	// 끝을 지나도 그림은 그대로 남는다. 화면을 넘기는 것은 서버의 로비 복귀 타이머다.
	return EMatchResultPhase::Hold;
}

float FMatchResultTimeline::GetTotalSeconds() const
{
	float Total = 0.0f;
	for (EMatchResultPhase Step = EMatchResultPhase::Frozen; Step != EMatchResultPhase::Idle; Step = GetNextPhase(Step))
	{
		Total += GetPhaseSeconds(Step);
	}
	return Total;
}

EMatchResultPhase FMatchResultTimeline::GetNextPhase(EMatchResultPhase Phase)
{
	switch (Phase)
	{
	case EMatchResultPhase::Frozen:     return EMatchResultPhase::FadingOut;
	case EMatchResultPhase::FadingOut:  return EMatchResultPhase::Reveal;
	case EMatchResultPhase::Reveal:     return EMatchResultPhase::BarEmpty;
	case EMatchResultPhase::BarEmpty:   return EMatchResultPhase::BarTeaser;
	case EMatchResultPhase::BarTeaser:  return EMatchResultPhase::BarReal;
	case EMatchResultPhase::BarReal:    return EMatchResultPhase::BarFinish;
	case EMatchResultPhase::BarFinish:  return EMatchResultPhase::Characters;
	case EMatchResultPhase::Characters: return EMatchResultPhase::Hold;
	default:                            return EMatchResultPhase::Idle;
	}
}

FMatchResultBarState FMatchResultMath::MakeBarState(EMatchResultPhase Phase, const FMatchResultInput& Result)
{
	FMatchResultBarState State;
	switch (Phase)
	{
	case EMatchResultPhase::BarTeaser:
		State.LeftCoverage = Result.TeaserCoverage;
		State.RightCoverage = Result.TeaserCoverage;
		return State;

	case EMatchResultPhase::BarReal:
		State.LeftCoverage = Result.LeftCoverage;
		State.RightCoverage = Result.RightCoverage;
		return State;

	case EMatchResultPhase::BarFinish:
	case EMatchResultPhase::Characters:
	case EMatchResultPhase::Hold:
		State.LeftCoverage = Result.LeftCoverage;
		State.RightCoverage = Result.RightCoverage;
		State.ForcedKnockoutTeam = Result.WinningTeam;
		return State;

	default:
		// Idle 부터 BarEmpty 까지는 빈 바다.
		return State;
	}
}

FTransform FMatchResultMath::MakeCharacterTransform(const FTransform& View, const FVector& SlotLocation, float Offset,
	float MeshYawOffset)
{
	const FVector Forward = View.GetUnitAxis(EAxis::X);

	// 카메라에서 자리로 향하는 시선 광선. 이동도 방향도 이 선 하나로 정해진다.
	//
	// 이동: 이 선 위를 움직인다. 정면 축과 나란한 선을 따라가면 원근 때문에 화면 위 자리가 중심에서
	// 바깥으로 벌어진다. 광선 위에서는 깊이만 배율로 바뀌므로 r/f, u/f 가 그대로 남아 화면 자리는
	// 고정되고 크기만 달라진다.
	//
	// 방향: 정면 축이 아니라 이 선의 반대를 본다. 정면 축과 나란히 세우면(스크린 정렬 빌보드) 화면
	// 가장자리에 선 캐릭터는 시선 광선이 비스듬해 바깥으로 돌아선 것처럼 보인다.
	const FVector ViewRay = (SlotLocation - View.GetLocation()).GetSafeNormal(UE_SMALL_NUMBER, Forward);

	// 머리는 카메라의 위쪽에 최대한 붙인다(MakeFromXZ 가 얼굴 방향에 맞춰 직교화한다). 뒤에 로컬 요를
	// 한 번 더 걸어 메시가 자기 공간에서 보는 쪽을 앞으로 돌린다.
	const FQuat Basis = FRotationMatrix::MakeFromXZ(-ViewRay, View.GetUnitAxis(EAxis::Z)).ToQuat();
	const FQuat MeshSpin(FVector::UpVector, FMath::DegreesToRadians(MeshYawOffset));

	return FTransform(Basis * MeshSpin, SlotLocation - ViewRay * Offset);
}

EMatchResultStep FMatchResultMath::GetStep(int32 SlotTeam, int32 WinningTeam)
{
	if (!Teams::IsValidId(WinningTeam) || !Teams::IsValidId(SlotTeam))
	{
		return EMatchResultStep::Stay;
	}
	return SlotTeam == WinningTeam ? EMatchResultStep::Approach : EMatchResultStep::Withdraw;
}
