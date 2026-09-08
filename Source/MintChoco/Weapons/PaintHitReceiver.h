#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PaintHitReceiver.generated.h"

struct FHitResult;

UINTERFACE(MinimalAPI, Blueprintable)
class UPaintHitReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something a paint hit can strike besides a paintable surface: a balloon, a target, a switch.
 *
 * Every paint source ends in FPaintDeposit::ApplyHit, which calls this on the authority before
 * it looks for a surface to paint, so a receiver hears every weapon (balls and the sniper ray)
 * with one hook and never has to know how it was hit. HitPower is the deposit's tuning value,
 * not a physical quantity; the receiver decides what it means.
 */
class MINTCHOCO_API IPaintHitReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Paint")
	void ReceivePaintHit(float HitPower, uint8 PaintId, const FHitResult& Hit);
};
