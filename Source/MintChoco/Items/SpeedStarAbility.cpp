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
#include "Weapons/PaintWeaponComponent.h"

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
	DropMark(Unit, *Star, LastMark, Direction, StarPaintId, StarGen);

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
		DropMark(*Unit, *Star, LastMark, Direction, StarPaintId, StarGen);
		Step = Now - LastMark;
		Step.Z = 0.0f;
	}
}

bool UGA_SpeedStar::DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At, const FVector& Direction, uint8 PaintId, uint8 StarGen)
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
	// 끌리게 한다. 속도 크기는 반경 계산에만 들어가니 수직 낙하 때와 같은 800을 유지한다.
	constexpr float ImpactSpeed = 800.0f;
	const float Cos = 1.0f / FMath::Max(Profile.TrailStretch, 1.0f);
	const float Sin = FMath::Sqrt(FMath::Max(1.0f - Cos * Cos, 0.0f));
	const FVector Flat = Direction.GetSafeNormal2D();
	const FVector Incident = Flat.IsNearlyZero()
		? FVector::DownVector
		: (-Flat * Sin - FVector::UpVector * Cos).GetSafeNormal();
	return Profile.TrailDeposit.ApplyHit(World, Hit, Incident * ImpactSpeed, PaintId, FMath::Rand(), StarGen);
}
