#pragma once

#include "CoreMinimal.h"
#include "Paint/PaintCellGrid.h"
#include "Subsystems/WorldSubsystem.h"

#include "PaintCoverageSubsystem.generated.h"

class UPaintableComponent;

/**
 * Registry of every paintable surface in the world and the place their coverage adds up. Score
 * lives here rather than on any one actor so a future server GameState has a single thing to ask.
 */
UCLASS()
class MINTCHOCO_API UPaintCoverageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPaintable(UPaintableComponent* Paintable);
	void UnregisterPaintable(UPaintableComponent* Paintable);

	/** Sum over every registered surface, in world cm^2. */
	UFUNCTION(BlueprintPure, Category = "Paint")
	FPaintCoverage GetWorldCoverage() const;

	TArray<UPaintableComponent*> GetPaintables() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UPaintableComponent>> Paintables;
};
