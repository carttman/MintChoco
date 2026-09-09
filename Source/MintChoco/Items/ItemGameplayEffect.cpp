#include "Items/ItemGameplayEffect.h"

#include "Items/ItemGameplayTags.h"

UItemGameplayEffect::UItemGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationCaller;
	DurationCaller.DataTag = ItemTags::Data_Item_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationCaller);

	// SetStackingType은 에디터 전용이고 DLL 밖으로 나오지도 않는다. 5.7이 비공개로 옮기겠다고
	// 표시한 멤버지만, 생성자에서 쓸 수 있는 길은 아직 이것뿐이다.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
}
