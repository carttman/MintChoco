#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PaintSettings.generated.h"

/**
 * Project-wide pieces of the paint pipeline that every paintable surface shares: how the score
 * grid is cut, how densely paint is stored, how the atlas fades at edges, and what a splat on a
 * direction nobody keeps turns into. Per-surface choices stay on the component and per-source
 * choices on the brush profile; only what is the same everywhere lives here. Everything is read
 * once, when a surface begins play - nothing in here changes during a match.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Paint"))
class MINTCHOCO_API UPaintSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPaintSettings();

	static const UPaintSettings& Get() { return *GetDefault<UPaintSettings>(); }

	/**
	 * World-space edge of one coverage cell. Cells are the gameplay layer's unit of ownership;
	 * the paint buffer keeps its own texel resolution regardless.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Score", meta = (ClampMin = "5"))
	float ScoreCellSize = 25.0f;

	/**
	 * World size of one paint texel the atlas aims for. Coarsened only when the islands would not
	 * fit into MaxRenderTargetSize, which the surface logs.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "0.05"))
	float PaintTexelSizeCm = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "64"))
	int32 MinRenderTargetSize = 256;

	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "64"))
	int32 MaxRenderTargetSize = 2048;

	/** Gutter around every island, in texels. Widened automatically to cover the edge fade and the distance range. */
	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "0"))
	int32 IslandPaddingTexels = 8;

	/**
	 * Footprint, in world cm^2, below which "floor follows world up" leaves a direction alone. A
	 * wall's top edge faces the sky too, but nobody scores on it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "0"))
	float AutoUpMinIslandArea = 2500.0f;

	/** Width of the displacement fade at island edges, in paint texels. */
	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "1"))
	float EdgeFadeTexels = 8.0f;

	/**
	 * Two neighbouring texels whose baked positions differ by more than this fraction of the
	 * mesh bounds belong to different surfaces, so the fade treats the gap as an edge.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Atlas", meta = (ClampMin = "0.001", ClampMax = "1"))
	float EdgeFadeSeamFraction = 0.05f;

	/**
	 * Cosmetic actor spawned where a splat lands on a direction its surface does not keep. It
	 * receives the splat through IPaintSplatEffect and owns its own look and lifetime; unset,
	 * such splats simply vanish.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Side Splats")
	TSoftClassPtr<AActor> SideSplatEffectClass;
};
