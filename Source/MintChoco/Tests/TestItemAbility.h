#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemProfile.h"

#include "TestItemAbility.generated.h"

/** UItemProfile은 추상이라 테스트가 직접 만들 수 있는 가장 작은 프로필. */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API UTestItemProfile : public UItemProfile
{
	GENERATED_BODY()
};

/**
 * 효과 시간과 마무리 시간을 테스트가 정하는 지속형 아이템. 상태 태그는 스위트 스피너의 것을 빌린다:
 * 무기가 막히고 풀리는 것까지 같은 경로로 확인하기 위해서다.
 *
 * 시간과 호출 횟수가 static인 이유: 인스턴스는 활성화할 때 CDO에서 만들어지므로 테스트가 미리
 * 손댈 수 없고, 아이템을 새로 줄 때마다 스펙과 인스턴스가 바뀐다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API UTestRecoveryItemAbility : public UItemAbility
{
	GENERATED_BODY()

public:
	UTestRecoveryItemAbility()
	{
		StateTag = ItemTags::State_Item_SweetSpinner;
		EffectClass = UGE_SweetSpinner::StaticClass();
	}

	static inline float EffectSeconds = 1.0f;
	static inline float RecoverySeconds = 0.5f;
	static inline int32 RecoveryStarts = 0;
	static inline int32 Ends = 0;

	static void ResetCounters()
	{
		RecoveryStarts = 0;
		Ends = 0;
	}

	using UItemAbility::FinishItem;

protected:
	virtual float GetEffectDuration(const UItemProfile& Profile) const override { return EffectSeconds; }
	virtual float GetRecoveryDuration(const UItemProfile& Profile) const override { return RecoverySeconds; }
	virtual void OnRecoveryStarted(AUnit& Unit, const UItemProfile& Profile) override { ++RecoveryStarts; }
	virtual void OnItemEnded(AUnit& Unit, const UItemProfile& Profile) override { ++Ends; }
};

/** 다른 아이템 하나. 즉발이라 켜지는 순간 끝난다: 켜지는 것 자체가 마무리를 끊는지만 본다. */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API UTestInstantItemAbility : public UItemAbility
{
	GENERATED_BODY()
};
