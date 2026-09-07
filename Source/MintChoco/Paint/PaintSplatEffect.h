#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintSplat.h"
#include "UObject/Interface.h"

#include "PaintSplatEffect.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPaintSplatEffect : public UInterface
{
	GENERATED_BODY()
};

/**
 * The cosmetic actor the paint subsystem spawns for a transient splat - one that landed on a
 * direction its surface does not keep. It is spawned at the contact, its Z along the surface
 * normal and its X along the stamp's U axis, then handed the splat: paint id for the team's
 * look, radius and stretch for the size, seed for variation. Everything else - the decal, the
 * drip, how long it lives - belongs to the implementing Blueprint.
 */
class MINTCHOCO_API IPaintSplatEffect
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Paint")
	void OnPaintSplat(const FPaintSplat& Splat);
};
