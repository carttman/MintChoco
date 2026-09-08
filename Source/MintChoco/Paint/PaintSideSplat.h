#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Paint/PaintSplatEffect.h"

#include "PaintSideSplat.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * The stock side-splat effect: a decal that stamps the same seeded blob the floor brush draws,
 * then runs down the wall and fades away. The paint subsystem spawns it for every transient
 * splat and hands the splat over through IPaintSplatEffect; nothing here touches a paint buffer
 * or the score.
 *
 * The decal projects along the actor's -Z (into the surface): its own X is the projection
 * axis, its Y follows the stamp's V axis and its Z the stamp's U axis, which is the frame the
 * decal material stamps in. A Blueprint child only picks the material, the team colors and the
 * timing.
 */
UCLASS(Blueprintable)
class MINTCHOCO_API APaintSideSplat : public AActor, public IPaintSplatEffect
{
	GENERATED_BODY()

public:
	APaintSideSplat();

	virtual void OnPaintSplat_Implementation(const FPaintSplat& Splat) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UDecalComponent> Decal;

	/**
	 * Decal material with the parameters M_PaintSideSplat declares: SplatColor, Radius, Stretch,
	 * Seed, ImpactU, BirthTime, Lifetime, DripLength.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** One color per paint id; an id past the end takes the last entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint")
	TArray<FLinearColor> TeamColors;

	/** Seconds from the contact until the decal is gone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0.1"))
	float Lifetime = 4.0f;

	/** How far the blob runs down the wall over its lifetime, in stamp radii. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0"))
	float DripLength = 1.5f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMID;
};
