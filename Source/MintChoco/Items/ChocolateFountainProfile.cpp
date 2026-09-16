#include "Items/ChocolateFountainProfile.h"

#include "Items/ChocolateFountain.h"
#include "MintChoco.h"
#include "Weapons/PaintballProfile.h"

float UChocolateFountainProfile::RadiusScaleForBurst(int32 BurstIndex) const
{
	const int32 Index = FMath::Max(BurstIndex, 0);
	if (!bScatterBursts)
	{
		return FMath::Pow(BurstGrowth, static_cast<float>(Index));
	}
	// 마지막 회차가 정확히 ScatterEndRadiusScale이 되도록 회차 수로 나눈다. 도포가 한 번뿐이면
	// 나눌 것이 없으므로 첫 회차의 1.0이 곧 마지막이다.
	const int32 Last = FMath::Max(BurstCount - 1, 1);
	const float Alpha = FMath::Clamp(static_cast<float>(Index) / static_cast<float>(Last), 0.0f, 1.0f);
	return FMath::Lerp(1.0f, ScatterEndRadiusScale, Alpha);
}

FPaintBurstParams UChocolateFountainProfile::MakeBurst(uint8 InPaintId, int32 Seed, float RadiusScale, int32 BurstIndex) const
{
	FPaintBurstParams Params = Burst;
	Params.PaintId = InPaintId;
	Params.Seed = Seed;
	const float Scale = FMath::Max(RadiusScale, 0.01f);

	if (bScatterBursts)
	{
		// 흩뿌림에서는 반경이 사거리를 직접 정하므로 Speed도 MinPitch/MaxPitch도 쓰이지 않는다.
		Params.ScatterRadius = Radius * Scale;
		Params.Count = FMath::Max(Burst.Count + FMath::Max(BurstIndex, 0) * ScatterCountStep, 1);
		return Params;
	}

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
