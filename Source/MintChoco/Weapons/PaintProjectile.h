#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintProjectile.generated.h"

class UPaintballProfile;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * A paintball in flight. It carries the profile that launched it and paints with its real impact
 * velocity, which is the one thing a hitscan has to fake. The visual mesh is set on the Blueprint;
 * radius and gravity come from the profile.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API APaintProjectile : public AActor
{
	GENERATED_BODY()

public:
	APaintProjectile();

	/** Call between SpawnActorDeferred and FinishSpawning: the movement component reads the velocity when it initializes. */
	void Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UPaintballProfile> Profile;

	uint8 PaintId = 0;
	int32 Seed = 0;
};
