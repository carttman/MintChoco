#include "Paint/PaintCoverageSubsystem.h"

#include "Paint/PaintableComponent.h"

void UPaintCoverageSubsystem::RegisterPaintable(UPaintableComponent* Paintable)
{
	if (Paintable != nullptr)
	{
		Paintables.AddUnique(Paintable);
	}
}

void UPaintCoverageSubsystem::UnregisterPaintable(UPaintableComponent* Paintable)
{
	Paintables.RemoveAll([Paintable](const TWeakObjectPtr<UPaintableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Paintable;
	});
}

FPaintCoverage UPaintCoverageSubsystem::GetWorldCoverage() const
{
	FPaintCoverage Coverage;
	for (const TWeakObjectPtr<UPaintableComponent>& Entry : Paintables)
	{
		if (const UPaintableComponent* const Paintable = Entry.Get())
		{
			Coverage.Add(Paintable->GetCoverage());
		}
	}
	return Coverage;
}

TArray<UPaintableComponent*> UPaintCoverageSubsystem::GetPaintables() const
{
	TArray<UPaintableComponent*> Result;
	for (const TWeakObjectPtr<UPaintableComponent>& Entry : Paintables)
	{
		if (UPaintableComponent* const Paintable = Entry.Get())
		{
			Result.Add(Paintable);
		}
	}
	return Result;
}

bool UPaintCoverageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
