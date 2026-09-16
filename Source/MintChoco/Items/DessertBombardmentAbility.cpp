#include "Items/DessertBombardmentAbility.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Game/TeamLook.h"
#include "Game/Unit.h"
#include "Items/DessertBombardmentProfile.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/PaintRain.h"
#include "MintChoco.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"

static TAutoConsoleVariable<int32> CVarBombardmentPreviewDebug(
	TEXT("mc.Bombardment.PreviewDebug"),
	0,
	TEXT("1이면 조준 미리보기가 잡은 띠를 선으로 그린다. 이펙트가 이 안을 채우지 못하면 에셋 쪽 문제다."),
	ECVF_Default);

namespace
{
	/**
	 * 미리보기 이펙트가 받는 유저 파라미터. 폭격 범위를 cm 그대로 건네므로, 스프라이트 크기를
	 * 이 값에 맞추면 표시와 실제로 떨어지는 띠가 정확히 겹친다. 팀 색은 다른 이펙트와 같은
	 * User.TintColor(TeamLook::NiagaraTintParameter)다.
	 */
	const FName PreviewLengthParameter(TEXT("User.Length"));
	const FName PreviewWidthParameter(TEXT("User.Width"));
}

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
		|| !Bombardment->AimPreviewFX)
	{
		return;
	}

	const FBombardmentPreviewShape Shape = ComputePreviewShape(Unit, *Bombardment);
	// 유닛에 붙이지 않는다: 띠는 몸통이 아니라 조준(컨트롤 요)을 따라가므로, 매 프레임
	// 우리가 옮기는 편이 몸통이 따로 도는 구간에서 어긋나지 않는다. 자동 파괴도 끈다 —
	// 조준이 끝나는 순간이 곧 수명의 끝이다.
	//
	// 프리컬 검사는 끈다. 띠의 중심은 맵 끝까지 폭격할 때 100 m 넘게 앞이라, 이펙트 타입에
	// 거리 컬링이 걸려 있으면 스폰 자체가 거부되고 nullptr이 온다 — 조준하는 본인에게 보여야
	// 하는 표시라 거리로 생략될 수 있는 연출이 아니다.
	//
	// 자동 활성화도 끈다. 켜진 뒤에 넣는 유저 파라미터는 다음 틱으로 미뤄지는데(SetVariable_Deferred),
	// 이 이펙트는 첫 틱에 스프라이트를 한 번 뿌리고 마는 구성이라 그 한 장이 에셋 기본 크기로
	// 태어나 버린다 — 자리는 우리가 잡은 길이대로인데 그림만 짧아, 플레이어와 화살표 사이가 벌어진다.
	// 파라미터를 먼저 넣고 아래에서 직접 켠다.
	Preview = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Bombardment->AimPreviewFX, Shape.Transform.GetLocation(), Shape.Transform.Rotator(),
		FVector::OneVector, /*bAutoDestroy=*/false, /*bAutoActivate=*/false,
		ENCPoolMethod::None, /*bPreCullCheck=*/false);
	if (!Preview)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 폭격 미리보기(%s)를 스폰하지 못했다."),
			*GetNameSafe(&Unit), *GetNameSafe(Bombardment->AimPreviewFX));
		return;
	}
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 폭격 미리보기 %s, 길이 %.0f cm × 폭 %.0f cm, 중심 %s."),
		*GetNameSafe(&Unit), *GetNameSafe(Bombardment->AimPreviewFX),
		Shape.Length, Shape.Width, *Shape.Transform.GetLocation().ToCompactString());

	// 팀 색은 조준 내내 바뀌지 않으므로 한 번만 넣는다. 알파는 팀 색에서 가져오지 않는다:
	// 그쪽 알파는 표면 머티리얼이 쓰지 않는 자리라 무엇이 들어 있든 표시의 투명도가 되면 안 된다.
	FLinearColor Tint = TeamLook::GetColor(GetPaintId(), World) * Bombardment->AimPreviewIntensity;
	Tint.A = 1.0f;
	Preview->SetVariableLinearColor(TeamLook::NiagaraTintParameter, Tint);
	ApplyPreviewShape(Shape);

	// 값이 다 들어간 뒤에 켠다. 여기서부터 첫 스프라이트가 제 길이로 태어난다.
	Preview->Activate(/*bReset=*/true);

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
	ApplyPreviewShape(ComputePreviewShape(*Unit, *Bombardment));
}

void UGA_DessertBombardment::ApplyPreviewShape(const FBombardmentPreviewShape& Shape)
{
	if (!Preview)
	{
		return;
	}
	Preview->SetWorldLocationAndRotation(Shape.Transform.GetLocation(), Shape.Transform.GetRotation());
	// 시스템이 이 파라미터를 읽지 않으면(에셋을 그대로 가져다 쓴 경우) 원래 크기로 그려질 뿐,
	// 없는 파라미터를 넣는 것 자체는 아무 일도 하지 않는다.
	Preview->SetVariableFloat(PreviewLengthParameter, Shape.Length);
	Preview->SetVariableFloat(PreviewWidthParameter, Shape.Width);

	DrawPreviewDebug(Shape);
}

void UGA_DessertBombardment::DrawPreviewDebug(const FBombardmentPreviewShape& Shape) const
{
#if ENABLE_DRAW_DEBUG
	const AUnit* const Unit = GetUnit();
	const UWorld* const World = Unit ? Unit->GetWorld() : nullptr;
	if (!World || CVarBombardmentPreviewDebug.GetValueOnGameThread() == 0)
	{
		return;
	}

	// 코드가 "여기에 이만큼 그려라"라고 말한 띠 그 자체다. 이펙트가 이 사각형을 채우지 못하면
	// 자리가 아니라 그림이 문제다 — 스프라이트가 User.Length/Width를 안 받고 있다는 뜻.
	const FVector Centre = Shape.Transform.GetLocation();
	const FVector Forward = Shape.Transform.GetUnitAxis(EAxis::X) * (Shape.Length * 0.5f);
	const FVector Right = Shape.Transform.GetUnitAxis(EAxis::Y) * (Shape.Width * 0.5f);
	const FVector Corners[4] = {
		Centre - Forward - Right, Centre + Forward - Right,
		Centre + Forward + Right, Centre - Forward + Right};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		DrawDebugLine(World, Corners[Index], Corners[(Index + 1) % 4], FColor::Green,
			/*bPersistentLines=*/false, /*LifeTime=*/-1.0f, /*DepthPriority=*/0, /*Thickness=*/3.0f);
	}
	// 중심(= 이펙트 컴포넌트가 놓인 자리)과 발밑을 잇는다. 이 선이 길면 띠가 발밑에서 떨어진 것이다.
	DrawDebugLine(World, Unit->GetActorLocation(), Centre, FColor::Yellow,
		false, -1.0f, 0, /*Thickness=*/2.0f);
	DrawDebugPoint(World, Centre, 12.0f, FColor::Red, false, -1.0f);
#endif
}

void UGA_DessertBombardment::OnAimEnded(AUnit& Unit, const UItemProfile& Profile, bool bConfirmed)
{
	DestroyPreview();
}

void UGA_DessertBombardment::DestroyPreview()
{
	if (Preview)
	{
		// 표시는 조준이 끝나는 순간 사라져야 한다. Deactivate로는 마지막 스프라이트가
		// 제 수명만큼 남아 확정 뒤에도 띠가 잠깐 보인다.
		Preview->DestroyComponent();
		Preview = nullptr;
	}
}

FBombardmentPreviewShape UGA_DessertBombardment::ComputePreviewShape(
	const AUnit& Unit, const UDessertBombardmentProfile& Profile) const
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

	// 폭격이 실제로 닿는 길이. 표시는 이보다 길어지지 않는다 — 맵 끝을 보고 있으면 표시도 같이 짧아진다.
	const float FullLength = FMath::Max(Rows * Profile.RowSpacing, Profile.RowSpacing);
	const float Length = Profile.AimPreviewLength > 0.0f
		? FMath::Min(Profile.AimPreviewLength, FullLength)
		: FullLength;
	const float Width = Profile.AimPreviewWidth > 0.0f
		? Profile.AimPreviewWidth
		: FMath::Max(Profile.Columns, 1) * Profile.ColumnSpacing;

	// 전부 플레이어 기준이다. 자리는 길이와 무관하게 AimPreviewDistance 하나로 정한다 —
	// 길이를 키워도 이펙트가 멀어지지 않는다. 길이는 이펙트가 알아서 쓸 값으로만 나간다.
	const float FeetZ = Origin.Z - Unit.GetSimpleCollisionHalfHeight();
	const FVector Centre = Origin + Forward * Profile.AimPreviewDistance;

	FBombardmentPreviewShape Shape;
	Shape.Transform = FTransform(Yaw, FVector(Centre.X, Centre.Y, FeetZ + Profile.AimPreviewHeight));
	Shape.Length = Length;
	Shape.Width = Width;
	return Shape;
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
	Params.LeadInSeconds = Fired->LeadInSeconds;
	Params.TelegraphFX = Fired->TelegraphFX;
	Params.TelegraphRowStride = Fired->TelegraphRowStride;
	Params.TelegraphHeight = Fired->TelegraphHeight;
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
