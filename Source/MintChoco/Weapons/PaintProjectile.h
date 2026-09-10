#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintProjectile.generated.h"

class UPaintballProfile;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Object channel "Paintball" from DefaultEngine.ini: what a ball is, so that balls can be told to ignore each other. */
inline constexpr ECollisionChannel PaintballChannel = ECC_GameTraceChannel1;

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

	/**
	 * Call between SpawnActorDeferred and FinishSpawning: the movement component reads the velocity
	 * when it initializes. A cosmetic ball is a client's picture of one the server owns: it flies
	 * and dies identically but leaves no paint.
	 */
	void Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity, bool bInCosmetic);

	/** 이 공이 칠하는 id(팀). 초콜릿 돔이 상대 탄을 가려낼 때 본다. */
	uint8 GetPaintId() const { return PaintId; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	/** Pawns are overlapped rather than blocked so a ball never pushes a player; the contact is handled like a hit. */
	UFUNCTION()
	void OnPawnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

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
	bool bCosmetic = false;
};
