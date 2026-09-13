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

void USweetSpinnerProfile::GetVolleyWindow(float& OutStart, float& OutEnd) const
{
	OutStart = 0.0f;
	OutEnd = Duration;

	float Start = 0.0f;
	float End = 0.0f;
	if (!SweetSpinner::FindVolleyWindow(SpinAnimation, Start, End))
	{
		return;
	}

	// 애니메이션은 어빌리티가 켜지는 순간부터 재생되므로 시퀀스의 시각이 곧 효과 안에서의 시각이다.
	// 지속시간을 넘는 구간은 잘라 낸다: 효과가 끝난 뒤에는 쏠 수 없다.
	OutStart = FMath::Min(Start, Duration);
	OutEnd = FMath::Min(End, Duration);
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

	// 회전 구간이 효과보다 늦게 끝나면 산탄이 잘린다. 효과가 끝나는 순간 상태 태그가 내려가
	// 자세도 같이 풀리므로, 뿌리는 도중에 애니메이션이 끊기고 평소 자세로 돌아간다.
	float SpinStart = 0.0f;
	float SpinEnd = 0.0f;
	if (SweetSpinner::FindVolleyWindow(SpinAnimation, SpinStart, SpinEnd))
	{
		UE_CLOG(SpinEnd > Duration, LogMintChoco, Warning,
			TEXT("%s: %s의 회전 구간이 %.2f초에 끝나는데 Duration은 %.2f초입니다. 효과가 먼저 끝나 산탄이 잘리고 자세도 풀립니다."),
			*GetNameSafe(Owner), *GetName(), SpinEnd, Duration);
		UE_CLOG(SpinStart >= Duration, LogMintChoco, Warning,
			TEXT("%s: %s의 회전 구간이 %.2f초에 시작하는데 Duration이 %.2f초라, 뿌리기도 전에 효과가 끝납니다."),
			*GetNameSafe(Owner), *GetName(), SpinStart, Duration);
	}
	if (Volley)
	{
		Volley->LogUnsetReferences(Owner);
	}
}
