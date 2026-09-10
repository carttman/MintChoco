#include "Items/SweetSpinnerProfile.h"

#include "MintChoco.h"
#include "Weapons/PaintGunProfile.h"

float SweetSpinner::VolleyYawDegrees(float StartYaw, int32 Index, int32 Count, float Turns)
{
	const float StepDeg = Count > 0 ? Turns * 360.0f / Count : 0.0f;
	return StartYaw + StepDeg * Index;
}

int32 USweetSpinnerProfile::GetVolleyCount() const
{
	return FMath::Max(1, FMath::RoundToInt(Duration / FMath::Max(VolleyInterval, UE_KINDA_SMALL_NUMBER)));
}

void USweetSpinnerProfile::LogUnsetReferences(const UObject* Owner) const
{
	Super::LogUnsetReferences(Owner);
	UE_CLOG(!Volley, LogMintChoco, Warning, TEXT("%s: %s has no Volley, the spinner will spin without painting."), *GetNameSafe(Owner), *GetName());
	if (Volley)
	{
		Volley->LogUnsetReferences(Owner);
	}
}
