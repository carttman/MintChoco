#include "Screen/ScreenFadeRelay.h"

#include "Screen/ScreenFadeSubsystem.h"

AScreenFadeRelay::AScreenFadeRelay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(10.0f);
}

void AScreenFadeRelay::MulticastFadeOut_Implementation(float Duration)
{
	if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(this))
	{
		Fade->FadeOut(Duration);
	}
}
