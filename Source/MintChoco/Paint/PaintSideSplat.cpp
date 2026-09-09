#include "Paint/PaintSideSplat.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Paint/PaintLog.h"

namespace
{
	const FName SplatColorParam(TEXT("SplatColor"));
	const FName RadiusParam(TEXT("Radius"));
	const FName StretchParam(TEXT("Stretch"));
	const FName SeedParam(TEXT("Seed"));
	const FName ImpactUParam(TEXT("ImpactU"));
	const FName BirthTimeParam(TEXT("BirthTime"));
	const FName LifetimeParam(TEXT("Lifetime"));
	const FName DripLengthParam(TEXT("DripLength"));
	const FName ReachParam(TEXT("Reach"));
	const FName DripDirParam(TEXT("DripDir"));
}

APaintSideSplat::APaintSideSplat()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// A decal projects along its own X. Pitching it down puts that X on the actor's -Z, into the
	// surface, and leaves its Z on the actor's X, the stamp's U axis.
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(GetRootComponent());
	Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	TeamColors = {
		FLinearColor(0.35f, 0.90f, 0.70f),
		FLinearColor(0.32f, 0.18f, 0.10f),
		FLinearColor(0.35f, 0.90f, 0.70f),
		FLinearColor(0.32f, 0.18f, 0.10f),
	};
}

void APaintSideSplat::OnPaintSplat_Implementation(const FPaintSplat& Splat)
{
	if (!DecalMaterial)
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no decal material, the side splat has nothing to show."), *GetName());
		Destroy();
		return;
	}

	// The stamp reaches Radius * Stretch along U and Radius along V, satellites almost to the
	// rim, and the drip runs below that: one generous box covers every orientation of "down".
	const float Reach = Splat.Radius * FMath::Max(Splat.Stretch, 1.0f) * (1.0f + DripLength);
	Decal->DecalSize = FVector(Splat.Radius, Reach, Reach);
	Decal->SetDecalMaterial(DecalMaterial);
	Decal->MarkRenderStateDirty();

	DecalMID = Decal->CreateDynamicMaterialInstance();
	if (DecalMID)
	{
		const FLinearColor Color = TeamColors.IsEmpty()
			? FLinearColor::White
			: TeamColors[FMath::Min<int32>(Splat.PaintId, TeamColors.Num() - 1)];
		DecalMID->SetVectorParameterValue(SplatColorParam, Color);
		DecalMID->SetScalarParameterValue(RadiusParam, Splat.Radius);
		DecalMID->SetScalarParameterValue(StretchParam, Splat.Stretch);
		DecalMID->SetScalarParameterValue(SeedParam, static_cast<float>(Splat.Seed));
		DecalMID->SetScalarParameterValue(ImpactUParam, Splat.ImpactU);
		// The material's Time node counts the same seconds as the world.
		DecalMID->SetScalarParameterValue(BirthTimeParam, GetWorld()->GetTimeSeconds());
		DecalMID->SetScalarParameterValue(LifetimeParam, Lifetime);
		DecalMID->SetScalarParameterValue(DripLengthParam, DripLength);
		// The decal material works in decal UVs, which span the box's local Y and Z out to Reach;
		// gravity arrives in the same frame so the drip can run down whichever way the wall faces.
		DecalMID->SetScalarParameterValue(ReachParam, Reach);
		const FVector LocalDown = Decal->GetComponentTransform().InverseTransformVectorNoScale(FVector::DownVector);
		DecalMID->SetVectorParameterValue(DripDirParam, FLinearColor(LocalDown.Y, LocalDown.Z, 0.0f));
	}

	SetLifeSpan(Lifetime + 0.1f);
}
