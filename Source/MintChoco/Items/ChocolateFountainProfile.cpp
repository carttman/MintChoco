#include "Items/ChocolateFountainProfile.h"

#include "Items/ChocolateFountain.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

FPaintBurstParams UChocolateFountainProfile::MakeBurst(uint8 InPaintId, int32 Seed) const
{
	FPaintBurstParams Params = Burst;
	Params.PaintId = InPaintId;
	Params.Seed = Seed;
	if (bBurstMatchesRadius && Params.Paintball)
	{
		Params.Speed = PaintBurst::SpeedForRange(Radius, Params.Paintball->GravityScale);
	}
	return Params;
}

void UChocolateFountainProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!DomeClass, LogMintChoco, Warning, TEXT("%s: %s has no DomeClass, nothing will appear."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(!Burst.Paintball, LogMintChoco, Warning,
		TEXT("%s: %s has no Burst.Paintball, the dome will not paint the ground under it."), *GetNameSafe(Owner), *GetName());
}
