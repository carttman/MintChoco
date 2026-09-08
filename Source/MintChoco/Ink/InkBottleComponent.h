#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

#include "InkBottleComponent.generated.h"

class UInkTankComponent;
class UMaterialInstanceDynamic;

/**
 * The liquid inside the ink bottle on a pawn's back; this component is the liquid mesh itself.
 *
 * It shows the owner's ink tank as the fill level and drives the material's slosh from the way the
 * bottle moves. Purely cosmetic: each machine wobbles its own copy from what it sees, and a
 * dedicated server never ticks it. The material contract is small on purpose - the code writes
 * only Fill, WobbleTilt, WobbleEnergy and LiquidHeight (plus SurfaceUpBlend when HorizonLock is
 * set), so everything else about the look stays tunable in the material instance.
 */
UCLASS(ClassGroup = (Paint), meta = (BlueprintSpawnableComponent))
class MINTCHOCO_API UInkBottleComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UInkBottleComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Swaps to the team's ink material. An id without a material keeps whatever is showing. */
	UFUNCTION(BlueprintCallable, Category = "Ink Bottle")
	void SetTeam(int32 TeamId);

	/**
	 * The disc that draws the liquid's top. This component places it on the surface plane every
	 * tick in world space, so it must use absolute location, rotation and scale; its material clips
	 * everything outside the bottle's radius.
	 */
	void SetSurfaceMesh(UStaticMeshComponent* InSurfaceMesh);

protected:
	/** One liquid material per paint id, in team order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink Bottle")
	TArray<TObjectPtr<UMaterialInterface>> TeamMaterials;

	/** One surface-disc material per paint id, in team order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ink Bottle")
	TArray<TObjectPtr<UMaterialInterface>> TeamSurfaceMaterials;

	/** Level shown when the owner has no ink tank. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle", meta = (ClampMin = "0", ClampMax = "1"))
	float DefaultFill = 1.0f;

	/** How fast the shown level chases the tank. Hides replication steps; 0 snaps. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle", meta = (ClampMin = "0"))
	float FillSmoothing = 8.0f;

	/**
	 * Overrides the material's SurfaceUpBlend: 0 tilts the surface with the bottle, 1 keeps it
	 * level with the world, values between do part of each. Negative leaves the material's value.
	 */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle", meta = (ClampMin = "-1", ClampMax = "1"))
	float HorizonLock = -1.0f;

	/** Spring pull of the surface back to flat. Higher sloshes faster. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float Stiffness = 60.0f;

	/** How quickly a slosh dies down. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float Damping = 4.0f;

	/** Steepest surface slope (rise over run) the slosh may reach. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float MaxTilt = 0.6f;

	/** Slosh push per cm/s^2 of the bottle's horizontal acceleration. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float LinearGain = 0.006f;

	/** Slosh push per rad/s of the bottle spinning about a horizontal axis. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float AngularGain = 0.1f;

	/** Ripple strength per unit of slosh speed; the material scales its ripples by the result. */
	UPROPERTY(EditAnywhere, Category = "Ink Bottle|Wobble", meta = (ClampMin = "0"))
	float EnergyGain = 0.5f;

private:
	void UpdateFill(float DeltaTime);
	void UpdateWobble(float DeltaTime);
	void UpdateSurface();
	void RebuildMaterial(UMaterialInterface* Base);
	void RebuildSurfaceMaterial(UMaterialInterface* Base);
	void PushRuntimeParameters();
	float ComputeLiquidHeight() const;
	float GetSurfaceUpBlend() const;
	FVector ComputeSurfaceNormal() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiquidMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	TWeakObjectPtr<UInkTankComponent> Tank;
	FVector PrevVelocity = FVector::ZeroVector;
	FQuat PrevRotation = FQuat::Identity;
	FVector2D Tilt = FVector2D::ZeroVector;
	FVector2D TiltVelocity = FVector2D::ZeroVector;
	float Energy = 0.0f;
	float DisplayedFill = 1.0f;
	bool bHasPrevious = false;
};
