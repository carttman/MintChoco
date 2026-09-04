#include "Paint/PaintSubsystem.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"

#include "Paint/PaintableComponent.h"

void UPaintSubsystem::RegisterPaintable(UPaintableComponent* Paintable)
{
	if (Paintable)
	{
		Paintables.AddUnique(Paintable);
	}
}

void UPaintSubsystem::UnregisterPaintable(UPaintableComponent* Paintable)
{
	Paintables.RemoveAll([Paintable](const TWeakObjectPtr<UPaintableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Paintable;
	});
}

void UPaintSubsystem::ApplySplat(const FPaintSplat& Splat)
{
	// A physics overlap rather than the registry: collision, not a bounding box, decides which
	// surfaces the stamp can reach, and it is the same query a projectile hit came from.
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(
		Overlaps, Splat.Location, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Splat.GetWorldExtent()));

	// Overlap results repeat an actor once per overlapping component, so dedupe on the
	// paintable itself before drawing.
	TSet<UPaintableComponent*> Painted;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* const Actor = Overlap.GetActor();
		UPaintableComponent* const Paintable =
			Actor ? Actor->FindComponentByClass<UPaintableComponent>() : nullptr;
		if (!Paintable)
		{
			continue;
		}

		bool bAlreadyPainted = false;
		Painted.Add(Paintable, &bAlreadyPainted);
		if (!bAlreadyPainted)
		{
			Paintable->ApplySplat(Splat);
		}
	}
}

FPaintCoverage UPaintSubsystem::GetWorldCoverage() const
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

TArray<UPaintableComponent*> UPaintSubsystem::GetPaintables() const
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

void UPaintSubsystem::SetDebugDraw(bool bText, bool bCells)
{
	for (UPaintableComponent* const Paintable : GetPaintables())
	{
		Paintable->SetDebugDraw(bText, bCells);
	}
}

bool UPaintSubsystem::IsAnyDebugTextDrawn() const
{
	return GetPaintables().ContainsByPredicate(
		[](const UPaintableComponent* Paintable) { return Paintable->IsDebugTextDrawn(); });
}

bool UPaintSubsystem::AreAnyDebugCellsDrawn() const
{
	return GetPaintables().ContainsByPredicate(
		[](const UPaintableComponent* Paintable) { return Paintable->AreDebugCellsDrawn(); });
}

bool UPaintSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
