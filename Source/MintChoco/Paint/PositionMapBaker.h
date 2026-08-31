#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PositionMapBaker.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

DECLARE_DELEGATE_OneParam(FOnPositionMapBaked, UTextureRenderTarget2D*);

/**
 * Bakes a world-position-unwrap map for one mesh: a capture-only proxy of the mesh wears the
 * unwrap material permanently, and an orthographic scene capture photographs it once the
 * material's PSOs have finished precaching. Owning that whole dance here keeps
 * UPaintableComponent about painting, not capture plumbing.
 *
 * Outer must be an object with a world (the owning component), because timers and component
 * registration both reach the world through the outer chain.
 */
UCLASS()
class MINTCHOCO_API UPositionMapBaker : public UObject
{
	GENERATED_BODY()

public:
	/** Fires once per successful bake, on the game thread, with the finished map. */
	FOnPositionMapBaked OnBaked;

	void Initialize(
		UMeshComponent* InSourceMesh,
		UMaterialInterface* InUnwrapMaterial,
		int32 InResolution,
		float InPlaneSize);

	/** Creates the proxy on first call, then bakes as soon as the PSO precache queue drains. */
	void RequestBake();

	/** Cancels any pending bake and tears the proxy down. Safe to call twice. */
	void Shutdown();

	UTextureRenderTarget2D* GetPositionMap() const { return PositionRenderTarget; }

private:
	void TryBake();
	void Bake();
	UTextureRenderTarget2D* CreatePositionBuffer();

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> SourceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> UnwrapMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> UnwrapMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ProxyMesh;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PositionRenderTarget;

	int32 Resolution = 1024;
	float PlaneSize = 1000.0f;
	FTimerHandle BakeWaitHandle;
	float BakeWaitElapsed = 0.0f;
};
