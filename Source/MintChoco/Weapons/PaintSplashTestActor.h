#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintSplashTestActor.generated.h"

class UPaintballProfile;

/**
 * Lands a paintball on whatever lies along its incident velocity, over and over, without a gun:
 * the same deposit, impact effect and splash a real ball leaves. Drop one into a test map to tune
 * the splash from the editor, or spawn it from a script to look at the effect without playing.
 */
UCLASS(Blueprintable)
class MINTCHOCO_API APaintSplashTestActor : public AActor
{
	GENERATED_BODY()

public:
	APaintSplashTestActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Lands one ball now. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void Fire();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	TObjectPtr<const UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 PaintId = 0;

	/** World velocity the ball arrives with; the surface is whatever this line meets first from the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint")
	FVector IncidentVelocity = FVector(1500.0, 0.0, -2500.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "10", ForceUnits = "cm"))
	float TraceLength = 500.0f;

	/** Seconds between landings; 0 lands once at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ForceUnits = "s"))
	float Interval = 2.0f;

private:
	FTimerHandle FireTimer;
};
