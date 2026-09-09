#include "Items/SpeedStarProfile.h"

#include "MintChoco.h"

void USpeedStarProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!TrailDeposit.CanPaint(), LogMintChoco, Warning, TEXT("%s: %s TrailDeposit has no BrushProfile, the trail will not paint."), *GetNameSafe(Owner), *GetName());
}
