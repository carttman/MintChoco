#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintSplat.h"

#include "PaintStar.generated.h"

/**
 * One team's speed-star paint as the server tracks it and every machine replicates it. Gen is the
 * generation the running (or latest) star's trail carries; while a star runs that trail is locked
 * and rainbow, and once the team's last star has ended it fades to the team look over FadeDuration.
 * Pure rules on server time, no world, so the whole sequence is unit-testable.
 */
USTRUCT()
struct MINTCHOCO_API FStarPaintState
{
	GENERATED_BODY()

	/** Generation the current trail writes. 0 until the team's first star. */
	UPROPERTY()
	uint8 Gen = 0;

	/** Stars running right now; two teammates can each hold one. */
	UPROPERTY()
	uint8 ActiveCount = 0;

	/** Server time the latest star runs out. The lock and the rainbow end here even if End never arrives. */
	UPROPERTY()
	double ActiveUntilServerTime = 0.0;

	/** Server time the last star ended, where the fade begins. */
	UPROPERTY()
	double EndServerTime = 0.0;

	UPROPERTY()
	float FadeDuration = 2.0f;

	/** A star starts. Returns the generation its trail writes. */
	uint8 Begin(double Now, float Duration, float InFadeDuration);

	void End(double Now);

	bool IsActive(double Now) const { return ActiveCount > 0 && Now < ActiveUntilServerTime; }

	/** The generation locked right now, or 0. */
	uint8 GetLockedGen(double Now) const { return IsActive(Now) ? Gen : 0; }

	/** Server time the rainbow starts fading: when the running star runs out, or when the last one ended. */
	double GetFadeStart() const { return ActiveCount > 0 ? ActiveUntilServerTime : EndServerTime; }

	/** Whether any trail of this team is still rainbow or fading. */
	bool IsVisible(double Now) const { return Gen != 0 && Now < GetFadeStart() + FadeDuration; }
};

/**
 * The star state as the shaders take it: per team id, in the local world clock the material Time
 * node counts. The surface material reads all three; the brush and the cell grid take the locks
 * derived from it at the moment a splat is accepted.
 */
struct FPaintStarShaderState
{
	float Gen[PaintTeamIdCount] = {};
	float FadeStart[PaintTeamIdCount] = {};
	float FadeDuration[PaintTeamIdCount] = {1.0f, 1.0f, 1.0f, 1.0f};

	FPaintLockGens GetLockGens(double LocalNow) const;
};
