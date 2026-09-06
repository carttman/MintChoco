#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSplat.h"
#include "Subsystems/WorldSubsystem.h"

#include "PaintSubsystem.generated.h"

class UPaintableComponent;

DECLARE_DELEGATE_OneParam(FPaintSplatSubmitted, const FPaintSplat&);

/**
 * The world's paint entry point. A paint source hands a finished splat to SubmitSplat on the
 * authority; whoever is bound to OnSplatSubmitted (the game state's replicated log) records it
 * and every machine, the server included, draws it through ApplySplat. Unbound, as in the
 * sample map or a standalone game, SubmitSplat draws straight away. The registry of paintable
 * surfaces adds their coverage up into the score.
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
	 * an actor boundary simply paints both sides. Local only: it does not replicate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ApplySplat(const FPaintSplat& Splat);

	/** Wipes every registered surface. Local only. */
	UFUNCTION(BlueprintCallable, Category = "Paint")
	void ClearPaint();

	/** Sum over every registered surface, in world cm^2. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	FPaintCoverage GetWorldCoverage() const;

	TArray<UPaintableComponent*> GetPaintables() const;

	/** Flips the coverage overlays on every registered surface at once. */
	UFUNCTION(BlueprintCallable, Category = "Paint|Debug")
	void SetDebugDraw(bool bText, bool bCells);

	bool IsAnyDebugTextDrawn() const;
	bool AreAnyDebugCellsDrawn() const;

	/** Bound by the authority's recorder of splats. Receives everything SubmitSplat accepts. */
	FPaintSplatSubmitted OnSplatSubmitted;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UPaintableComponent>> Paintables;
};
