#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplat.h"
#include "Subsystems/WorldSubsystem.h"

#include "PaintSubsystem.generated.h"

class UPaintableComponent;

/**
 * The world's paint entry point. A paint source hands a finished splat here and every surface
 * inside its extent draws and scores it; the registry of paintable surfaces adds their coverage
 * up into the score. Replication will sit on this one class: the server owns ApplySplat, the
 * clients replay what it sends.
 */
UCLASS()
class MINTCHOCO_API UPaintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPaintable(UPaintableComponent* Paintable);
	void UnregisterPaintable(UPaintableComponent* Paintable);

	/**
	 * Applies one splat to every paintable surface within its extent. The brush works in world
	 * space, so every surface tests its own texels against the same stamp and a splat landing on
	 * an actor boundary simply paints both sides.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	/** Sum over every registered surface, in world cm^2. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	FPaintCoverage GetWorldCoverage() const;

	TArray<UPaintableComponent*> GetPaintables() const;

	/** Flips the coverage overlays on every registered surface at once. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Debug")
	void SetDebugDraw(bool bText, bool bCells);

	bool IsAnyDebugTextDrawn() const;
	bool AreAnyDebugCellsDrawn() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UPaintableComponent>> Paintables;
};
