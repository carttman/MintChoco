#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "AbilityTask_Tick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityTaskTickSignature, float, DeltaTime);

/**
 * 어빌리티가 살아 있는 동안 매 프레임 OnTick을 낸다. 어빌리티가 끝나면 태스크가 같이
 * 정리되므로 회전이나 자국처럼 "효과 중 계속 하는 일"을 여기에 걸면 정리 코드가 필요 없다.
 */
UCLASS()
class MINTCHOCO_API UAbilityTask_Tick : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_Tick();

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_Tick* TickEveryFrame(UGameplayAbility* OwningAbility);

	UPROPERTY(BlueprintAssignable)
	FAbilityTaskTickSignature OnTick;

	virtual void TickTask(float DeltaTime) override;
};
