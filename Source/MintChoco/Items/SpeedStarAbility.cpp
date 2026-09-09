#include "Items/SpeedStarAbility.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

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

	LastMark = Unit.GetActorLocation();
	DropMark(Unit, *Star, LastMark);

	UAbilityTask_Tick* const Tick = UAbilityTask_Tick::TickEveryFrame(this);
	Tick->OnTick.AddDynamic(this, &UGA_SpeedStar::HandleTick);
	Tick->ReadyForActivation();
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
		LastMark += Step.GetSafeNormal() * Spacing;
		LastMark.Z = Now.Z;
		DropMark(*Unit, *Star, LastMark);
		Step = Now - LastMark;
		Step.Z = 0.0f;
	}
}

bool UGA_SpeedStar::DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At)
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

	const uint8 PaintId = Unit.GetPaintWeapon() ? Unit.GetPaintWeapon()->GetPaintId() : 0;
	// 바로 아래로 떨어진 것처럼 취급해 자국이 늘어나지 않는 둥근 도장이 된다.
	return Profile.TrailDeposit.ApplyHit(World, Hit, FVector(0.0f, 0.0f, -800.0f), PaintId, FMath::Rand());
}
