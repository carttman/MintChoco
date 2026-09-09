#include "Items/DessertBombardmentAbility.h"

#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/DessertBombardmentProfile.h"
#include "Items/PaintRain.h"
#include "MintChoco.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"

FBox UGA_DessertBombardment::ComputeMapBounds(const UWorld& World)
{
	FBox Bounds(ForceInit);
	const UPaintSubsystem* const Paint = World.GetSubsystem<UPaintSubsystem>();
	if (!Paint)
	{
		return Bounds;
	}
	for (const UPaintableComponent* const Paintable : Paint->GetPaintables())
	{
		// 벽만 있는 표면은 맵의 폭을 넓히지 않는다. 바닥(위를 향한 면)이 맵이다.
		if (Paintable && Paintable->IsDirectionEnabled(EPaintFaceDirection::Up))
		{
			Bounds += Paintable->GetWorldBounds();
		}
	}
	return Bounds;
}

void UGA_DessertBombardment::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UDessertBombardmentProfile* const Bombardment = Cast<UDessertBombardmentProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Bombardment || !Bombardment->Paintball || !World || !IsAuthority())
	{
		return;
	}

	const FBox Bounds = ComputeMapBounds(*World);
	if (!Bounds.IsValid)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 도색 가능 표면이 없어 폭격 범위를 잴 수 없다."), *GetNameSafe(&Unit));
		return;
	}

	const FRotator Yaw(0.0f, Unit.GetControlRotation().Yaw, 0.0f);
	const FVector Forward = Yaw.Vector();
	const FVector Origin = Unit.GetActorLocation();

	FPaintRainParams Params;
	Params.Origin = Origin;
	Params.Direction = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	Params.RowSpacing = Bombardment->RowSpacing;
	Params.Columns = Bombardment->Columns;
	Params.ColumnSpacing = Bombardment->ColumnSpacing;
	Params.DropZ = Bounds.Max.Z + Bombardment->DropHeight;
	Params.DropSpeed = Bombardment->DropSpeed;
	Params.Interval = Bombardment->RowInterval;
	Params.Paintball = Bombardment->Paintball;
	Params.PaintId = GetPaintId();
	Params.Seed = FMath::Rand();
	Params.RowCount = FPaintRainPlan::CountRows(
		FVector2D(Origin.X, Origin.Y), Params.Direction,
		FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)),
		Params.RowSpacing, Bombardment->MaxRows);

	if (Params.RowCount <= 0)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 폭격 방향에 맵이 없다."), *GetNameSafe(&Unit));
		return;
	}

	APaintRain::Spawn(*World, Params, Bombardment->RainClass);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 디저트 폭격 %d행 × %d열."), *GetNameSafe(&Unit), Params.RowCount, Params.Columns);
}
