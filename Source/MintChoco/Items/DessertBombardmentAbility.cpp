#include "Items/DessertBombardmentAbility.h"

#include "Engine/World.h"

#include "Game/Unit.h"
#include "Items/DessertBombardmentProfile.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/PaintRain.h"
#include "MintChoco.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"

void UGA_DessertBombardment::OnAimStarted(AUnit& Unit, const UItemProfile& Profile)
{
	Bombardment = Cast<UDessertBombardmentProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Bombardment || !World)
	{
		return;
	}
	CachedBounds = ComputeMapBounds(*World);

	// 경로는 조준하는 본인만 본다(리슨 호스트 포함). 서버의 인스턴스도 조준 상태로
	// 들어가지만 그릴 것은 없다 — 확정이 올 때까지 기다리는 것이 서버의 몫이다.
	if (!Unit.IsLocallyControlled() || World->GetNetMode() == NM_DedicatedServer
		|| !Bombardment->AimPreviewClass)
	{
		return;
	}

	FActorSpawnParameters Spawn;
	Spawn.Owner = &Unit;
	Spawn.Instigator = &Unit;
	// 발밑에서 시작하므로 바닥과 겹친다. 미리보기는 연출이라 밀려나면 안 된다.
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Preview = World->SpawnActor<AActor>(
		Bombardment->AimPreviewClass, ComputePreviewTransform(Unit, *Bombardment), Spawn);
	if (!Preview)
	{
		return;
	}

	UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
	Tick->OnTick.AddDynamic(this, &UGA_DessertBombardment::HandleTick);
	Tick->ReadyForActivation();
}

void UGA_DessertBombardment::HandleTick(float DeltaTime)
{
	const AUnit* const Unit = GetUnit();
	if (!Preview || !Unit || !Bombardment)
	{
		return;
	}
	Preview->SetActorTransform(ComputePreviewTransform(*Unit, *Bombardment));
}

void UGA_DessertBombardment::OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed)
{
	DestroyPreview();
}

void UGA_DessertBombardment::DestroyPreview()
{
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}

FTransform UGA_DessertBombardment::ComputePreviewTransform(const AUnit& Unit, const UDessertBombardmentProfile& Profile) const
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
			Profile.RowSpacing, Profile.MaxRows);
	}

	const float Length = FMath::Max(Rows * Profile.RowSpacing, Profile.RowSpacing);
	const float Width = FMath::Max(Profile.Columns, 1) * Profile.ColumnSpacing;

	// 기본 큐브는 한 변 100 cm에 중심이 원점이다. 중간 지점에 놓고 스케일하면 발밑에서 앞으로 뻗는다.
	constexpr float CubeSide = 100.0f;
	constexpr float Thickness = 4.0f;
	constexpr float GroundOffset = 2.0f;
	const FVector Centre = Origin + Forward * (Length * 0.5f) + FVector(0.0f, 0.0f, GroundOffset - Unit.GetSimpleCollisionHalfHeight());
	return FTransform(Yaw, Centre, FVector(Length / CubeSide, Width / CubeSide, Thickness / CubeSide));
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

void UGA_DessertBombardment::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	const UDessertBombardmentProfile* const Fired = Cast<UDessertBombardmentProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Fired || !Fired->Paintball || !World || !IsAuthority())
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
	Params.RowSpacing = Fired->RowSpacing;
	Params.Columns = Fired->Columns;
	Params.ColumnSpacing = Fired->ColumnSpacing;
	Params.DropZ = Bounds.Max.Z + Fired->DropHeight;
	Params.DropSpeed = Fired->DropSpeed;
	Params.Interval = Fired->RowInterval;
	Params.Paintball = Fired->Paintball;
	Params.PaintId = GetPaintId();
	Params.Seed = FMath::Rand();
	Params.RowCount = FPaintRainPlan::CountRows(
		FVector2D(Origin.X, Origin.Y), Params.Direction,
		FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)),
		Params.RowSpacing, Fired->MaxRows);

	if (Params.RowCount <= 0)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 폭격 방향에 맵이 없다."), *GetNameSafe(&Unit));
		return;
	}

	APaintRain::Spawn(*World, Params, Fired->RainClass);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 디저트 폭격 %d행 × %d열."), *GetNameSafe(&Unit), Params.RowCount, Params.Columns);
}
