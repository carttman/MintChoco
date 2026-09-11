#include "Items/SpeedStarAbility.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

#include "Game/GameGameState.h"
#include "Game/Unit.h"
#include "Items/AbilityTask_Tick.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/SpeedStarProfile.h"
#include "Paint/PaintBrushProfile.h"
#include "Weapons/PaintWeaponComponent.h"

namespace
{
	/** 자국이 바닥에 떨어지는 가짜 속도(cm/s). 브러시 반지름에만 들어간다. */
	constexpr float TrailImpactSpeed = 800.0f;
}

UGA_SpeedStar::UGA_SpeedStar()
{
	StateTag = ItemTags::State_Item_SpeedStar;
	EffectClass = UGE_SpeedStar::StaticClass();
}

void UGA_SpeedStar::OnItemActivated(AUnit& Unit, const UItemProfile& Profile)
{
	Star = Cast<USpeedStarProfile>(&Profile);

	// 자국은 페인트라 서버만 찍는다. 스플랫 로그가 클라이언트에 나른다.
	if (!Star || !IsAuthority())
	{
		return;
	}

	StarPaintId = Unit.GetPaintWeapon() ? Unit.GetPaintWeapon()->GetPaintId() : 0;
	StarGen = 0;
	// 첫 자국보다 먼저 알린다. 스플랫이 제출되는 순간의 잠금에 자기 세대가 들어 있어야 한다.
	if (AGameGameState* const State = Unit.GetWorld()->GetGameState<AGameGameState>())
	{
		StarState = State;
		StarGen = State->BeginStarPaint(StarPaintId, Profile.Duration, Star->PaintFadeDuration);
		bStarPaintBegun = true;
	}

	LastMark = Unit.GetActorLocation();
	// 아직 안 움직였으면 바라보는 쪽으로 번진다.
	FVector Direction = Unit.GetVelocity().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Unit.GetActorForwardVector().GetSafeNormal2D();
	}
	// 첫 자국은 뒤에 자국이 없으니 둥글게 시작한다.
	LastDirection = Direction;
	StraightRun = 0.0f;
	PlaceMark(Unit, LastMark, Direction);

	UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
	Tick->OnTick.AddDynamic(this, &UGA_SpeedStar::HandleTick);
	Tick->ReadyForActivation();
}

void UGA_SpeedStar::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// OnItemEnded는 유닛이 남아 있을 때만 오므로, 잠금은 어떤 경로로 끝나든 여기서 푼다.
	if (bStarPaintBegun)
	{
		bStarPaintBegun = false;
		if (AGameGameState* const State = StarState.Get())
		{
			State->EndStarPaint(StarPaintId);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SpeedStar::HandleTick(float DeltaTime)
{
	AUnit* const Unit = GetUnit();
	if (!Unit || !Star)
	{
		return;
	}

	// 한 틱에 간격보다 멀리 갔으면(낮은 프레임률) 그 사이도 빠짐없이 찍는다. 높이는
	// 트레이스가 바닥에서 다시 찾으므로 수평 거리만 센다.
	const FVector Now = Unit->GetActorLocation();
	FVector Step = Now - LastMark;
	Step.Z = 0.0f;
	const float Spacing = FMath::Max(Star->MarkSpacing, 5.0f);
	while (Step.SizeSquared() >= FMath::Square(Spacing))
	{
		// 자국마다 그 구간의 진행 방향을 쓰니 휘어 달리면 자국도 따라 휜다.
		const FVector Direction = Step.GetSafeNormal();
		LastMark += Direction * Spacing;
		LastMark.Z = Now.Z;
		PlaceMark(*Unit, LastMark, Direction);
		Step = Now - LastMark;
		Step.Z = 0.0f;
	}
}

void UGA_SpeedStar::PlaceMark(AUnit& Unit, const FVector& At, const FVector& Direction)
{
	// 꺾인 각도만큼 직진 거리를 깎는다: 직각이면 0(둥근 자국), 완만한 곡선은 거의 그대로.
	StraightRun *= FMath::Max(FVector::DotProduct(LastDirection, Direction), 0.0f);
	DropMark(Unit, *Star, At, Direction, StretchForRun(*Star, StraightRun), StarPaintId, StarGen);
	StraightRun += FMath::Max(Star->MarkSpacing, 5.0f);
	LastDirection = Direction;
}

float UGA_SpeedStar::StretchForRun(const USpeedStarProfile& Profile, float StraightRun)
{
	const UPaintBrushProfile* const Brush = Profile.TrailDeposit.BrushProfile;
	if (!Brush)
	{
		return 1.0f;
	}
	const float Radius = Brush->ComputeRadius(Profile.TrailDeposit.SplatVolume, TrailImpactSpeed);
	return FMath::Min(Profile.TrailStretch, Brush->StretchWithinTail(Radius, Radius + FMath::Max(StraightRun, 0.0f)));
}

bool UGA_SpeedStar::DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At, const FVector& Direction, float Stretch, uint8 PaintId, uint8 StarGen)
{
	UWorld* const World = Unit.GetWorld();
	if (!World || !Profile.TrailDeposit.CanPaint())
	{
		return false;
	}

	const float HalfHeight = Unit.GetCapsuleComponent() ? Unit.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector Start = At;
	const FVector End = At - FVector(0.0f, 0.0f, HalfHeight + 60.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpeedStarTrail), /*bTraceComplex=*/true, &Unit);
	Params.bReturnFaceIndex = true;

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}

	// 브러시는 1/cos(입사각)만큼 접선 방향으로 늘리고 중심을 진행 쪽으로 미니, 원하는 배율이 나오는
	// 각도로 비스듬히 떨어진 것처럼 꾸민다. 뒤에서 앞으로 날아든 것처럼 뒤집어 넘겨서 자국이 발 뒤로
	// 끌리게 한다. 속도 크기는 반경 계산에만 들어가니 수직 낙하 때와 같은 값을 유지한다.
	const float Cos = 1.0f / FMath::Max(Stretch, 1.0f);
	const float Sin = FMath::Sqrt(FMath::Max(1.0f - Cos * Cos, 0.0f));
	const FVector Flat = Direction.GetSafeNormal2D();
	const FVector Incident = Flat.IsNearlyZero()
		? FVector::DownVector
		: (-Flat * Sin - FVector::UpVector * Cos).GetSafeNormal();
	return Profile.TrailDeposit.ApplyHit(World, Hit, Incident * TrailImpactSpeed, PaintId, FMath::Rand(), 1.0f, StarGen);
}
