#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PaintMapBaker.generated.h"

class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

DECLARE_DELEGATE_TwoParams(FOnPaintMapsBaked, UTextureRenderTarget2D* /*PositionMap*/, UTextureRenderTarget2D* /*EdgeFadeMap*/);

/**
 * Bakes the two per-mesh maps the paint pipeline needs, once: the world-position unwrap (a
 * capture-only proxy of the mesh wears the unwrap material and an orthographic capture
 * photographs it once the material's PSOs have finished precaching) and, from that, the
 * island edge fade. Owning that whole dance here keeps UPaintableComponent about painting,
 * not capture plumbing.
 *
 * Outer must be an object with a world (the owning component), because timers and component
 * registration both reach the world through the outer chain.
 */
UCLASS()
class MINTCHOCO_API UPaintMapBaker : public UObject
{
	GENERATED_BODY()

public:
	/** Fires once per successful bake, on the game thread. The edge fade map is null if its material is unset. */
	FOnPaintMapsBaked OnBaked;

	void Initialize(
		UStaticMeshComponent* InSourceMesh,
		int32 InResolution,
		float InPlaneSize,
		float InFadeTexels,
		float InSeamFraction);

	/** Creates the proxy on first call, then bakes as soon as the PSO precache queue drains. */
	void RequestBake();

	/** Cancels any pending bake and tears down the proxy and capture. Safe to call twice. */
	void Shutdown();

	UTextureRenderTarget2D* GetPositionMap() const { return PositionRenderTarget; }
	UTextureRenderTarget2D* GetEdgeFadeMap() const { return EdgeFadeRenderTarget; }

private:
	static UTextureRenderTarget2D* CreatePositionBuffer(UObject* Outer, int32 Resolution);
	static UTextureRenderTarget2D* CreateFadeBuffer(UObject* Outer, int32 Resolution);
	static USceneCaptureComponent2D* CreateCaptureComponent(
		UStaticMeshComponent* ProxyMesh, float OrthoWidth, UTextureRenderTarget2D* Target);

	void TryBake();
	void Bake();
	bool IsPositionMapValid() const;
	void BakeEdgeFade();
	void EnsurePositionBuffer();
	void EnsureCaptureComponent();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SourceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> UnwrapMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EdgeFadeMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ProxyMesh;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PositionRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> EdgeFadeRenderTarget;

	int32 Resolution = 1024;
	float PlaneSize = 1000.0f;
	float FadeTexels = 8.0f;
	float SeamFraction = 0.05f;
	FTimerHandle BakeWaitHandle;
	float BakeWaitElapsed = 0.0f;
	float LastAttemptElapsed = -1.0f;
};
