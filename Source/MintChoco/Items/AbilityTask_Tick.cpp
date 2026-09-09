#include "Items/AbilityTask_Tick.h"

UAbilityTask_Tick::UAbilityTask_Tick()
{
	bTickingTask = true;
}

UAbilityTask_Tick* UAbilityTask_Tick::TickEveryFrame(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<UAbilityTask_Tick>(OwningAbility);
}

void UAbilityTask_Tick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTick.Broadcast(DeltaTime);
	}
}
