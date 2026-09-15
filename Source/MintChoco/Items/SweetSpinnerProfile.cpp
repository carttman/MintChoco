#include "Items/SweetSpinnerProfile.h"

#include "Animation/AnimSequenceBase.h"

#include "Items/SpinnerVolleyWindow.h"
#include "MintChoco.h"
#include "Weapons/PaintGunProfile.h"

float SweetSpinner::VolleyYawDegrees(float StartYaw, int32 Index, int32 Count, float Turns)
{
	const float StepDeg = Count > 0 ? Turns * 360.0f / Count : 0.0f;
	return StartYaw + StepDeg * Index;
}

bool SweetSpinner::FindVolleyWindow(const UAnimSequenceBase* Sequence, float& OutStart, float& OutEnd)
{
	if (!Sequence)
	{
		return false;
	}

	// 노티파이는 에셋에 박힌 데이터라 포즈를 돌리지 않아도 읽힌다. 그래서 메시를 애니메이션하지
	// 않는 데디케이티드 서버에서도 같은 구간이 나온다.
	for (const FAnimNotifyEvent& Event : Sequence->Notifies)
	{
		if (!Event.NotifyStateClass || !Event.NotifyStateClass->IsA<UAnimNotifyState_SpinnerVolley>())
		{
			continue;
		}

		OutStart = FMath::Max(Event.GetTriggerTime(), 0.0f);
		OutEnd = Event.GetEndTriggerTime();
		return OutEnd > OutStart;
	}
	return false;
}

void SweetSpinner::ComposeVolleyWindow(float StartLength, float SpinLength, float InnerStart, float InnerEnd,
	float Duration, float& OutStart, float& OutEnd)
{
	// 회전은 시작 동작이 끝난 뒤부터다. 표시가 있으면 그 안, 없으면 회전 구간 전체.
	const float SpinBegin = FMath::Max(StartLength, 0.0f);
	const float Inner = FMath::Clamp(InnerStart, 0.0f, FMath::Max(SpinLength, 0.0f));
	const float InnerStop = FMath::Clamp(InnerEnd, Inner, FMath::Max(SpinLength, 0.0f));

	// 지속시간 밖으로는 나갈 수 없다: 효과가 끝나면 상태 태그가 내려가 자세도 발사도 멈춘다.
	OutStart = FMath::Clamp(SpinBegin + Inner, 0.0f, FMath::Max(Duration, 0.0f));
	OutEnd = FMath::Clamp(SpinBegin + InnerStop, OutStart, FMath::Max(Duration, 0.0f));
}

void SweetSpinner::ComposeRecovery(float StartLength, float SpinLength, float EndLength, float Duration,
	float& OutEffect, float& OutRecovery)
{
	const float SafeDuration = FMath::Max(Duration, 0.0f);
	const float SpinFinish = FMath::Clamp(FMath::Max(StartLength, 0.0f) + FMath::Max(SpinLength, 0.0f), 0.0f, SafeDuration);
	const float Remaining = SafeDuration - SpinFinish;

	// 마무리가 없으면 효과가 끝까지 간다. 회전 자세가 지속시간이 끝날 때까지 이어진다.
	if (EndLength <= UE_KINDA_SMALL_NUMBER || Remaining <= UE_KINDA_SMALL_NUMBER || SpinFinish <= UE_KINDA_SMALL_NUMBER)
	{
		OutEffect = SafeDuration;
		OutRecovery = 0.0f;
		return;
	}

	// 끝 클립은 한 번만 돈다. 자세 시퀀스는 애님 그래프에서 루프하므로, 남은 시간이 더 길다고
	// 늘려 두면 끝 동작이 되감겨 다시 시작한다.
	OutEffect = SpinFinish;
	OutRecovery = FMath::Min(EndLength, Remaining);
}

float USweetSpinnerProfile::GetStartPhaseLength() const
{
	return StartAnimation ? FMath::Min(StartAnimation->GetPlayLength(), Duration) : 0.0f;
}

float USweetSpinnerProfile::GetSpinPhaseLength() const
{
	// 회전 클립이 없으면 남은 시간 전부가 회전이다(예전 동작).
	const float Remaining = FMath::Max(Duration - GetStartPhaseLength(), 0.0f);
	return SpinAnimation ? FMath::Min(SpinAnimation->GetPlayLength(), Remaining) : Remaining;
}

void USweetSpinnerProfile::GetVolleyWindow(float& OutStart, float& OutEnd) const
{
	const float SpinLength = GetSpinPhaseLength();

	float InnerStart = 0.0f;
	float InnerEnd = SpinLength;
	SweetSpinner::FindVolleyWindow(SpinAnimation, InnerStart, InnerEnd);

	SweetSpinner::ComposeVolleyWindow(GetStartPhaseLength(), SpinLength, InnerStart, InnerEnd, Duration, OutStart, OutEnd);
}

float USweetSpinnerProfile::GetEffectPhaseLength() const
{
	float Effect = 0.0f;
	float Recovery = 0.0f;
	SweetSpinner::ComposeRecovery(GetStartPhaseLength(), GetSpinPhaseLength(),
		EndAnimation ? EndAnimation->GetPlayLength() : 0.0f, Duration, Effect, Recovery);
	return Effect;
}

float USweetSpinnerProfile::GetEndPhaseLength() const
{
	float Effect = 0.0f;
	float Recovery = 0.0f;
	SweetSpinner::ComposeRecovery(GetStartPhaseLength(), GetSpinPhaseLength(),
		EndAnimation ? EndAnimation->GetPlayLength() : 0.0f, Duration, Effect, Recovery);
	return Recovery;
}

int32 USweetSpinnerProfile::GetVolleyCount(float Window) const
{
	return FMath::Max(1, FMath::RoundToInt(Window / FMath::Max(VolleyInterval, UE_KINDA_SMALL_NUMBER)));
}

int32 USweetSpinnerProfile::GetVolleyCount() const
{
	return GetVolleyCount(Duration);
}

void USweetSpinnerProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!Volley, LogMintChoco, Warning, TEXT("%s: %s has no Volley, the spinner will spin without painting."), *GetNameSafe(Owner), *GetName());

	// 세 구간의 합이 지속시간을 넘으면 뒤가 잘린다. 효과가 끝나는 순간 상태 태그가 내려가
	// 자세도 산탄도 함께 멈추므로, 뿌리는 도중에 애니메이션이 끊기고 평소 자세로 돌아간다.
	const float StartLength = StartAnimation ? StartAnimation->GetPlayLength() : 0.0f;
	const float SpinLength = SpinAnimation ? SpinAnimation->GetPlayLength() : 0.0f;
	const float EndLength = EndAnimation ? EndAnimation->GetPlayLength() : 0.0f;
	const float Total = StartLength + SpinLength + EndLength;

	UE_CLOG(Total > Duration + UE_KINDA_SMALL_NUMBER, LogMintChoco, Warning,
		TEXT("%s: %s의 동작 길이 합이 %.2f초(시작 %.2f + 회전 %.2f + 끝 %.2f)인데 Duration은 %.2f초입니다. 뒤가 잘립니다."),
		*GetNameSafe(Owner), *GetName(), Total, StartLength, SpinLength, EndLength, Duration);

	UE_CLOG(StartLength >= Duration, LogMintChoco, Warning,
		TEXT("%s: %s의 시작 동작이 %.2f초인데 Duration이 %.2f초라, 돌기도 전에 효과가 끝납니다."),
		*GetNameSafe(Owner), *GetName(), StartLength, Duration);
	if (Volley)
	{
		Volley->LogUnsetReferences(Owner);
	}
}
