#include "Audio/SoundBank.h"

const FSoundEvent* USoundBank::Find(const FGameplayTag& Tag) const
{
	return Tag.IsValid() ? Events.Find(Tag) : nullptr;
}
