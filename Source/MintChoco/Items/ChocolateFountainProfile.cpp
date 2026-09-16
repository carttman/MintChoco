#include "Items/ChocolateFountainProfile.h"

#include "Items/ChocolateFountain.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

FPaintBurstParams UChocolateFountainProfile::MakeBurst(uint8 InPaintId, int32 Seed, float RadiusScale) const
{
	FPaintBurstParams Params = Burst;
	Params.PaintId = InPaintId;
	Params.Seed = Seed;
	const float Scale = FMath::Max(RadiusScale, 0.01f);
	if (bBurstMatchesRadius && Params.Paintball)
	{
		// 속도가 사거리를 정하므로 배율을 반경에 곱하면 도포 범위가 그대로 따라온다.
		Params.Speed = PaintBurst::SpeedForRange(Radius * Scale, Params.Paintball->GravityScale);
	}
	else
	{
		// 반경을 안 따라가는 설정(손으로 잡은 속도)에서는 속도에 배율을 그대로 곱한다.
		// 수평으로 쏘는 도포(MinPitch = MaxPitch = 0)는 떨어지는 시간이 속도와 무관하므로
		// 날아간 거리가 속도에 비례한다. 45도 포물선의 v² 관계와는 다르다.
		Params.Speed *= Scale;
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
