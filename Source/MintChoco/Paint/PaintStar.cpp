#include "Paint/PaintStar.h"

uint8 FStarPaintState::Begin(double Now, float Duration, float InFadeDuration)
{
	// Stars whose End never came (an owner destroyed mid-effect) count as over once they ran out.
	if (ActiveCount > 0 && Now >= ActiveUntilServerTime)
	{
		ActiveCount = 0;
		EndServerTime = ActiveUntilServerTime;
	}

	// A star starting while the last trail is still fading (a retrigger, or a teammate right after)
	// takes that trail back: same generation, so the whole trail lights up and locks again. Only a
	// fully faded trail is left behind as plain paint under a new generation.
	if (ActiveCount == 0 && (Gen == 0 || Now > EndServerTime + FadeDuration))
	{
		Gen = NextPaintStarGen(Gen);
	}
	++ActiveCount;
	ActiveUntilServerTime = FMath::Max(ActiveUntilServerTime, Now + Duration);
	FadeDuration = InFadeDuration;
	return Gen;
}

void FStarPaintState::End(double Now)
{
	if (ActiveCount == 0)
	{
		return;
	}
	if (--ActiveCount == 0)
	{
		EndServerTime = FMath::Min(Now, ActiveUntilServerTime);
	}
}

FPaintLockGens FPaintStarShaderState::GetLockGens(double LocalNow) const
{
	FPaintLockGens Locks;
	for (int32 Id = 0; Id < PaintTeamIdCount; ++Id)
	{
		Locks.Gen[Id] = LocalNow < FadeStart[Id] ? static_cast<uint8>(Gen[Id]) : 0;
	}
	return Locks;
}
