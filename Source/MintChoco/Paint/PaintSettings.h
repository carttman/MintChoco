#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PaintSettings.generated.h"

class UMaterialInterface;

/**
 * Project-wide pieces of the paint pipeline that every paintable surface shares: the materials
 * that bake a mesh's position map and edge fade. Per-surface choices stay on the component
 * and per-source choices on the brush profile; only what is the same everywhere lives here.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Paint"))
class MINTCHOCO_API UPaintSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPaintSettings();

	static const UPaintSettings& Get() { return *GetDefault<UPaintSettings>(); }

	/**
	 * Unlit material whose WPO flattens a mesh into its UV1 layout while the emissive outputs
	 * the pre-offset local position, normalized to the object bounds (M_PaintUnwrap).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bake")
	TSoftObjectPtr<UMaterialInterface> UnwrapMaterial;

	/**
	 * Unlit material that writes, per texel, how close the unwrap island edge is (M_PaintEdgeFade).
	 * Displacement is scaled by it: at a hard edge the neighbouring faces rise along different
	 * vertex normals, so any height left there tears the mesh open.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bake")
	TSoftObjectPtr<UMaterialInterface> EdgeFadeMaterial;
};
