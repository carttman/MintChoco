#include "Items/DessertBombardmentAbility.h"

#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/DessertBombardmentProfile.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/PaintRain.h"
#include "MintChoco.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"

UGA_DessertBombardment::UGA_DessertBombardment()
{
	// 조준 중이라는 상태 하나면 충분하다: 무기를 막는 것도, 좌클릭을 가로채는 것도 이 태그를 본다.
	StateTag = ItemTags::State_Item_Aiming;
	EffectClass = UGE_DessertBombardment::StaticClass();
}

void UGA_DessertBombardment::OnAimBegan(AUnit& Unit, const UItemProfile& Profile)
{
	if (const UWorld* const World = Unit.GetWorld())
	{
		CachedBounds = ComputeMapBounds(*World);
	}
}

FTransform UGA_DessertBombardment::ComputePreviewTransform(const AUnit& Unit, const UDessertBombardmentProfile& Bombardment) const
{
	const FRotator Yaw(0.0f, Unit.GetControlRotation().Yaw, 0.0f);
	const FVector Forward = Yaw.Vector();
	const FVector Origin = Unit.GetActorLocation();

	// 실제 발사와 같은 계산으로 행 수를 구한다. 조준선이 맵 밖으로 나가지 않는다.
	int32 Rows = 0;
	if (CachedBounds.IsValid)
	{
		Rows = FPaintRainPlan::CountRows(
			FVector2D(Origin.X, Origin.Y), FVector2D(Forward.X, Forward.Y).GetSafeNormal(),
			FBox2D(FVector2D(CachedBounds.Min.X, CachedBounds.Min.Y), FVector2D(CachedBounds.Max.X, CachedBounds.Max.Y)),
			Bombardment.RowSpacing, Bombardment.MaxRows);
	}

	const float Length = FMath::Max(Rows * Bombardment.RowSpacing, Bombardment.RowSpacing);
	const float Width = FMath::Max(Bombardment.Columns, 1) * Bombardment.ColumnSpacing;

	// 기본 큐브는 한 변 100 cm에 중심이 원점이다. 중간 지점에 놓고 스케일하면 발밑에서 앞으로 뻗는다.
	constexpr float CubeSide = 100.0f;
	constexpr float Thickness = 4.0f;
	constexpr float GroundOffset = 2.0f;
	const FVector Centre = Origin + Forward * (Length * 0.5f) + FVector(0.0f, 0.0f, GroundOffset - Unit.GetSimpleCollisionHalfHeight());
	return FTransform(Yaw, Centre, FVector(Length / CubeSide, Width / CubeSide, Thickness / CubeSide));
}

AActor* UGA_DessertBombardment::SpawnPreview(AUnit& Unit, const UItemProfile& Profile)
{
	const UDessertBombardmentProfile* const Bombardment = Cast<UDessertBombardmentProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Bombardment || !Bombardment->AimPreviewClass || !World)
	{
		return nullptr;
	}

	FActorSpawnParameters Spawn;
	Spawn.Owner = &Unit;
	Spawn.Instigator = &Unit;
	// 발밑에서 시작하므로 바닥과 겹친다. 미리보기는 연출이라 밀려나면 안 된다.
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(Bombardment->AimPreviewClass, ComputePreviewTransform(Unit, *Bombardment), Spawn);
}

void UGA_DessertBombardment::UpdatePreview(AUnit& Unit, AActor& InPreview, float DeltaTime)
{
	const UDessertBombardmentProfile* const Bombardment = Cast<UDessertBombardmentProfile>(GetItemProfile());
	if (!Bombardment)
	{
		return;
	}
	InPreview.SetActorTransform(ComputePreviewTransform(Unit, *Bombardment));
}

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

void UGA_DessertBombardment::OnAimConfirmed(AUnit& Unit, const UItemProfile& Profile)
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
