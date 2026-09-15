#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterfaceExport.h"
#include "Subsystems/WorldSubsystem.h"

#include "PaintSplashSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UPaintSplashProfile;
namespace PaintSplash
{
	struct FDroplet;
}
class UPaintSplashProfile;
class UPaintSubsystem;

/** One paintball contact that may splash, as the landing hands it over. */
struct FPaintSplashRequest
{
	const UPaintSplashProfile* Profile = nullptr;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	/** Velocity of the ball at the contact, cm/s, pointing into the surface. */
	FVector IncidentVelocity = FVector::ZeroVector;
	float BallRadius = 6.0f;
	/** Radius of the ball's own splat, cm; a droplet landing inside it leaves no mark. 0 when unknown. */
	float SplatRadius = 0.0f;
	uint8 PaintId = 0;
	int32 Seed = 0;
	/** False for a contact whose splat would not stick (a pawn, a movable mesh): the droplets still fly but leave no marks. */
	bool bLeavesMarks = true;
};

/**
 * Receives the landings of one splash from NS_PaintSplash (its Export Particle Data module, one
 * call per frame with every droplet that touched down) and stamps each as a draw-only splat with
 * the droplet brush. Position is the contact, Velocity carries the surface normal and Size the
 * droplet's launch speed, which is near enough its landing speed to size the mark.
 */
UCLASS()
class MINTCHOCO_API UPaintSplashLandingHandler : public UObject, public INiagaraParticleCallbackHandler
{
	GENERATED_BODY()

public:
	void Arm(const FPaintSplashRequest& Request, UPaintSubsystem* Paint, uint32 LockGens, float ExpiresAt);
	void Release();

	bool IsArmed() const { return bArmed; }
	bool HasExpired(float Now) const { return !bArmed || Now >= ExpiresAt; }
	int32 GetLandingCount() const { return Landings; }

	virtual void ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data, UNiagaraSystem* NiagaraSystem, const FVector& SimulationPositionOffset) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UPaintSplashProfile> Profile;

	TWeakObjectPtr<UPaintSubsystem> Paint;
	FVector ImpactPoint = FVector::ZeroVector;
	/** Landings closer than this to the contact leave no mark: the ball's splat already covers them. */
	float MarkClearance = 0.0f;
	uint8 PaintId = 0;
	uint32 LockGens = 0;
	int32 Seed = 0;
	float ExpiresAt = 0.0f;
	int32 Landings = 0;
	bool bArmed = false;
};

/**
 * The effect half of a paintball splash. At the contact it throws the droplets (the same
 * PaintSplash::GenerateDroplets the score uses), hands them to NS_PaintSplash slot by slot and
 * books a landing handler; the effect flies them, collides them and reports where they came down,
 * and the handler stamps the marks. Nothing replicates: every machine draws its own droplets and
 * marks, while the score comes from the phantom landings in UPaintSubsystem::ApplySplat.
 *
 * A dedicated server has no picture to keep and books nothing.
 */
UCLASS()
class MINTCHOCO_API UPaintSplashSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UPaintSplashSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Books the landings of one contact. Returns the handler the effect should report to, or null
	 * when no marks will be drawn here: a dedicated server, marks disabled, a request that leaves
	 * none, a profile with no droplet brush, or every handler busy.
	 */
	UPaintSplashLandingHandler* BeginSplash(const FPaintSplashRequest& Request);

	/**
	 * Writes the User parameters NS_PaintSplash reads: the droplets, the flight and look knobs and
	 * the handler, and releases the handler when the effect finishes.
	 */
	void ConfigureEffect(UNiagaraComponent& Effect, const FPaintSplashRequest& Request, UPaintSplashLandingHandler* Handler);

	int32 GetArmedHandlerCount() const;

	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Whether this machine draws marks at all: never a dedicated server, and only while the cvar allows. */
	bool DrawsMarks() const;

	/** A handler nobody holds: a free or expired one from the pool, or a new one under the cap. */
	UPaintSplashLandingHandler* AcquireHandler(float Now);

	UFUNCTION()
	void HandleEffectFinished(UNiagaraComponent* Effect);

	/** The blob's material for one splash: the profile's BlobMaterial with the droplets, the flight numbers and the team written in. Null without one. */
	static UMaterialInstanceDynamic* BuildBlobMaterial(UNiagaraComponent& Effect, const UPaintSplashProfile& Profile, const FPaintSplashRequest& Request,
		TArrayView<const PaintSplash::FDroplet> Droplets, const FVector& BlobScale, float GravityZ);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPaintSplashLandingHandler>> Handlers;

	/** Which handler each live effect reports to, so a finished effect frees its handler early. */
	TMap<TWeakObjectPtr<UNiagaraComponent>, TWeakObjectPtr<UPaintSplashLandingHandler>> Active;
};
