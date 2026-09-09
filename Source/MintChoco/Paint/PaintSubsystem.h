#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintIslandLayout.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintStar.h"
#include "Subsystems/WorldSubsystem.h"

#include "PaintSubsystem.generated.h"

class UPaintableComponent;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
struct FPaintAtlasBakeOutput;

DECLARE_DELEGATE_OneParam(FPaintSplatSubmitted, const FPaintSplat&);

/** A baked paint atlas: where every texel of a surface's paint buffer sits on the mesh, and how close to an edge. */
USTRUCT()
struct FPaintAtlas
{
	GENERATED_BODY()

	/** Bounds-normalized local position per texel, point sampled. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PositionMap;

	/** 0 at surface edges, 1 well inside, so displacement can die out before it tears. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> EdgeFadeMap;

	FPaintIslandLayout Layout;
};

DECLARE_DELEGATE_OneParam(FPaintAtlasReady, const FPaintAtlas&);

/**
 * The world's paint entry point. A paint source hands a finished splat to SubmitSplat on the
 * authority; whoever is bound to OnSplatSubmitted (the game state's replicated log) records it
 * and every machine, the server included, draws it through ApplySplat. Unbound, as in the
 * sample map or a standalone game, SubmitSplat draws straight away. The registry of paintable
 * surfaces adds their coverage up into the score.
 *
 * It also owns what surfaces share: baked atlases, one per mesh and layout rather than one per
 * actor, and the scratch render target the brush draws its rectangles into before they are
 * copied to a surface's own buffer.
 */
UCLASS()
class MINTCHOCO_API UPaintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPaintable(UPaintableComponent* Paintable);
	void UnregisterPaintable(UPaintableComponent* Paintable);

	/**
	 * The authority's entry point for a new splat. A client cannot submit; it only draws what the
	 * server sends. Routes to OnSplatSubmitted when bound, otherwise applies locally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void SubmitSplat(const FPaintSplat& Splat);

	/**
	 * Draws one splat on every paintable surface within its extent. The brush works in world
	 * space, so every surface tests its own texels against the same stamp and a splat landing on
	 * an actor boundary simply paints both sides. A transient splat touches no surface at all;
	 * it only spawns the side-splat effect. Local only: it does not replicate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	/** Wipes every registered surface. Local only. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/**
	 * The speed-star state every surface material reads - which generation is rainbow per team and
	 * when it fades - in this world's clock. Pushed by whoever replicates it (the game state) and
	 * handed to each surface as it begins play.
	 */
	void SetStarPaint(const FPaintStarShaderState& State);
	const FPaintStarShaderState& GetStarPaint() const { return StarPaint; }

	/** Which generation each team has locked at this moment; stamped into every splat SubmitSplat accepts. */
	FPaintLockGens GetLockGens() const;

	/** Sum over every registered surface, in world cm^2. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	FPaintCoverage GetWorldCoverage() const;

	TArray<UPaintableComponent*> GetPaintables() const;

	/**
	 * Hands back the atlas for this mesh and layout, baking it on a worker thread first if no
	 * surface has asked for it yet. OnReady fires on the game thread, right away for a cached
	 * atlas. Two surfaces with the same mesh, scale and directions share one atlas.
	 */
	void RequestAtlas(
		const UStaticMeshComponent& Mesh,
		int32 MaterialSlot,
		const FBox& LocalBounds,
		const FPaintIslandLayout& Layout,
		FPaintAtlasReady OnReady);

	/** The shared paint buffer the brush draws into before its rectangles are copied out. One per size. */
	UTextureRenderTarget2D* GetScratchTarget(int32 Size);

	/**
	 * A paint buffer: per texel R holds a paint id plus a speed-star generation (EncodePaintTexel),
	 * G the accumulated height, B the distance to the nearest paint edge. Point sampled and linear,
	 * since ids must never be interpolated and sRGB would corrupt the id round trip. Every buffer
	 * the brush copies between comes from here, so the formats always match.
	 */
	static UTextureRenderTarget2D* CreatePaintBuffer(UObject* Outer, int32 Size);

	/** Flips the coverage overlays on every registered surface at once. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Debug")
	void SetDebugDraw(bool bText, bool bCells);

	bool IsAnyDebugTextDrawn() const;
	bool AreAnyDebugCellsDrawn() const;

	/** Bound by the authority's recorder of splats. Receives everything SubmitSplat accepts. */
	FPaintSplatSubmitted OnSplatSubmitted;

protected:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	static FString MakeAtlasKey(const UStaticMesh& Mesh, const FPaintIslandLayout& Layout, float FadeTexels, float SeamFraction);

	/** Game thread: turns a finished bake into textures, caches them and wakes the surfaces that waited. */
	void FinishAtlas(const FString& Key, const FPaintIslandLayout& Layout, const FPaintAtlasBakeOutput& Output);

	/** Spawns the project's side-splat effect at the contact. Nothing on a dedicated server, which has no eyes. */
	void SpawnSideSplatEffect(const FPaintSplat& Splat);

	TArray<TWeakObjectPtr<UPaintableComponent>> Paintables;

	/** The settings' effect class once loaded; a project without one simply shows nothing. */
	UPROPERTY(Transient)
	TSubclassOf<AActor> SideSplatEffectClass;

	UPROPERTY(Transient)
	TMap<FString, FPaintAtlas> AtlasCache;

	/** Surfaces waiting on a bake that is running, by atlas key. */
	TMap<FString, TArray<FPaintAtlasReady>> PendingAtlases;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTextureRenderTarget2D>> ScratchTargets;

	FPaintStarShaderState StarPaint;
};
