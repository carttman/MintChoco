#include "Items/BeeProjectile.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SphereComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

#include "Game/Unit.h"
#include "Items/ItemSlotComponent.h"
#include "Items/BeeProfile.h"
#include "Items/ItemAreaEffect.h"
#include "Items/ItemSettings.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"
#include "Weapons/PaintProjectile.h"

namespace
{
	/** 고른 회피 방향을 유지하는 시간(초). 매 틱 다시 고르면 후보 사이를 오간다. */
	constexpr float AvoidanceCommitTime = 0.3f;
}

// ---------------------------------------------------------------- FBeeSteering

void FBeeSteering::BuildCandidates(const FVector& Desired, TArray<FVector>& OutCandidates)
{
	OutCandidates.Reset(8);
	const FVector Forward = Desired.GetSafeNormal();
	OutCandidates.Add(Forward);

	for (const float Yaw : { 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f })
	{
		OutCandidates.Add(Forward.RotateAngleAxis(Yaw, FVector::UpVector).GetSafeNormal());
	}

	// 위로 45도: 수평 성분과 위를 같은 비율로.
	FVector Flat = Forward;
	Flat.Z = 0.0f;
	if (Flat.Normalize())
	{
		OutCandidates.Add((Flat * FMath::Cos(FMath::DegreesToRadians(45.0f)) + FVector::UpVector * FMath::Sin(FMath::DegreesToRadians(45.0f))).GetSafeNormal());
	}
	else
	{
		OutCandidates.Add(FVector::UpVector);
	}
	OutCandidates.Add(FVector::UpVector);
}

FVector FBeeSteering::Choose(const TArray<FVector>& Candidates, const TArray<bool>& Blocked)
{
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (!Blocked.IsValidIndex(Index) || !Blocked[Index])
		{
			return Candidates[Index];
		}
	}
	return FVector::UpVector;
}

FVector FBeeSteering::TurnTowards(const FVector& Current, const FVector& Desired, float MaxAngleDeg)
{
	const FVector From = Current.GetSafeNormal();
	const FVector To = Desired.GetSafeNormal();
	if (From.IsNearlyZero())
	{
		return To;
	}
	if (To.IsNearlyZero())
	{
		return From;
	}
	return FMath::VInterpNormalRotationTo(From, To, 1.0f, MaxAngleDeg);
}

float FBeeSteering::VerticalComponent(float AltitudeError, float Scale, float MaxRise)
{
	const float SafeScale = FMath::Max(Scale, 1.0f);
	return FMath::Clamp(AltitudeError / SafeScale, -MaxRise, MaxRise);
}

FVector FBeeSteering::Combine(const FVector& Flat, float Vertical)
{
	const float V = FMath::Clamp(Vertical, -1.0f, 1.0f);
	FVector Horizontal(Flat.X, Flat.Y, 0.0f);
	if (!Horizontal.Normalize())
	{
		return FVector(0.0f, 0.0f, V >= 0.0f ? 1.0f : -1.0f);
	}
	return Horizontal * FMath::Sqrt(1.0f - V * V) + FVector(0.0f, 0.0f, V);
}

FVector FBeeSteering::TurnTowardsSplit(const FVector& Current, const FVector& WantedFlat, float WantedVertical, float MaxAngleDeg)
{
	FVector CurrentFlat(Current.X, Current.Y, 0.0f);
	FVector TargetFlat(WantedFlat.X, WantedFlat.Y, 0.0f);
	if (!TargetFlat.Normalize())
	{
		TargetFlat = CurrentFlat.GetSafeNormal();
	}
	if (!CurrentFlat.Normalize())
	{
		CurrentFlat = TargetFlat;
	}

	// 수평면 안의 회전: 두 벡터가 수평이라 회전축이 Z다.
	const FVector NewFlat = CurrentFlat.IsNearlyZero() ? TargetFlat : TurnTowards(CurrentFlat, TargetFlat, MaxAngleDeg);

	// Z 성분은 각도만큼만 바뀐다. 정확히는 sin이지만 작은 각도에서 라디안과 같다.
	const float CurrentVertical = FMath::Clamp(static_cast<float>(Current.Z), -1.0f, 1.0f);
	const float Step = FMath::DegreesToRadians(FMath::Max(MaxAngleDeg, 0.0f));
	const float NewVertical = FMath::FInterpConstantTo(CurrentVertical, FMath::Clamp(WantedVertical, -1.0f, 1.0f), 1.0f, Step);
	return Combine(NewFlat, NewVertical);
}

float FBeeSteering::MaxDescent(float Clearance, float Scale, float MaxDive)
{
	const float SafeScale = FMath::Max(Scale, 1.0f);
	return FMath::Clamp(Clearance / SafeScale, 0.0f, 1.0f) * MaxDive;
}

// ---------------------------------------------------------------- ABeeProjectile

AUnit* ABeeProjectile::FindNearestOpponent(const UWorld& World, const FVector& From, int32 Team, const AActor* Exclude)
{
	AUnit* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AUnit> It(&World); It; ++It)
	{
		AUnit* const Candidate = *It;
		if (!Candidate || !FItemAreaEffect::ShouldAffect(Candidate->GetTeam(), Team, Candidate == Exclude))
		{
			continue;
		}
		const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), From);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Candidate;
		}
	}
	return Nearest;
}

ABeeProjectile::ABeeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Sphere->SetSphereRadius(30.0f);
	Movement->ProjectileGravityScale = 0.0f;

	// 탄이 맞는 껍질. 뿌리 구는 탄을 무시하므로 탄에 밀려 멈추지 않고, 탄은 이 껍질에 막혀 터진다.
	Shell = CreateDefaultSubobject<USphereComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Sphere);
	Shell->InitSphereRadius(30.0f);
	Shell->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shell->SetCollisionObjectType(ECC_WorldDynamic);
	Shell->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shell->SetCollisionResponseToChannel(PaintballChannel, ECR_Block);
	Shell->SetGenerateOverlapEvents(false);
}

void ABeeProjectile::SetProfile(const UBeeProfile* InProfile)
{
	Profile = InProfile;
}

void ABeeProjectile::SetTarget(AUnit* InTarget)
{
	Target = InTarget;
}

void ABeeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// 클라이언트 복사본은 그림이다. 연출 탄이 껍질에 걸리면 서버와 그림이 어긋나므로 껍질도 끈다.
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	// 어빌리티 없이 놓인 꿀벌(레벨에 배치한 테스트용)은 설정의 프로필과 가장 가까운 유닛을 스스로 고른다.
	if (!Profile)
	{
		TArray<UItemProfile*> Items;
		UItemSettings::Get().LoadItems(Items);
		for (const UItemProfile* const Item : Items)
		{
			if (const UBeeProfile* const Bee = Cast<UBeeProfile>(Item))
			{
				Profile = Bee;
				break;
			}
		}
	}
	if (!Target.IsValid())
	{
		Target = FindNearestOpponent(*GetWorld(), GetActorLocation(), GetTeam(), GetInstigator());
	}

	SetActorTickEnabled(true);
	LastMark = GetActorLocation();
	if (Profile)
	{
		GetWorldTimerManager().SetTimer(LifeTimer, this, &ABeeProjectile::Expire, Profile->Lifetime, /*bLoop=*/false);
	}
}

float ABeeProjectile::GetDamageFraction() const
{
	return Profile && Profile->Health > 0.0f ? FMath::Clamp(Damage / Profile->Health, 0.0f, 1.0f) : 0.0f;
}

void ABeeProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority() || bDetonated || !Profile)
	{
		return;
	}
	Steer(DeltaTime);
	DropTrail();
}

bool ABeeProjectile::IsBlocked(const FVector& Direction) const
{
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeeProbe), /*bTraceComplex=*/false, this);
	Params.AddIgnoredActor(GetInstigator());
	if (Target.IsValid())
	{
		Params.AddIgnoredActor(Target.Get());
	}
	const FVector Start = GetActorLocation();
	FHitResult Hit;
	// 폰은 5.8에서 Visibility를 무시하므로 벽과 바닥만 장애물이다.
	return World->SweepSingleByChannel(Hit, Start, Start + Direction * Profile->ProbeDistance, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()), Params);
}

void ABeeProjectile::Steer(float DeltaTime)
{
	const FVector Location = GetActorLocation();
	const FVector Current = Movement->Velocity.IsNearlyZero() ? GetActorForwardVector() : Movement->Velocity.GetSafeNormal();

	// 수평 방향과 고도는 따로 정한다. 조준이 바닥 쪽을 향한다고 바닥을 장애물로 세면 매 틱 꺾였다 돌아온다.
	FVector ToTarget = Current * 1000.0f;
	float TargetZ = Location.Z;
	if (Target.IsValid())
	{
		ToTarget = Target->GetActorLocation() - Location;
		TargetZ = Target->GetActorLocation().Z;
	}
	FVector Flat(ToTarget.X, ToTarget.Y, 0.0f);
	const float HorizontalDistance = Flat.Size();
	if (!Flat.Normalize())
	{
		Flat = FVector(Current.X, Current.Y, 0.0f).GetSafeNormal();
		if (Flat.IsNearlyZero())
		{
			Flat = GetActorForwardVector().GetSafeNormal2D();
		}
	}

	// 수평 회피. 한 번 고른 방향은 잠시 유지한다: 후보를 매 틱 다시 고르면 둘 사이를 오간다.
	FVector Wanted = Flat;
	bool bClimbCandidate = false;
	if (AvoidanceTimeLeft > 0.0f && !IsBlocked(AvoidanceDirection))
	{
		AvoidanceTimeLeft -= DeltaTime;
		Wanted = AvoidanceDirection;
		bClimbCandidate = AvoidanceDirection.Z > KINDA_SMALL_NUMBER;
	}
	else
	{
		AvoidanceTimeLeft = 0.0f;
		if (IsBlocked(Flat))
		{
			TArray<FVector> Candidates;
			FBeeSteering::BuildCandidates(Flat, Candidates);
			TArray<bool> Blocked;
			Blocked.Reserve(Candidates.Num());
			Blocked.Add(true);
			for (int32 Index = 1; Index < Candidates.Num(); ++Index)
			{
				const bool bBlocked = IsBlocked(Candidates[Index]);
				Blocked.Add(bBlocked);
				if (!bBlocked)
				{
					break;
				}
			}
			Wanted = FBeeSteering::Choose(Candidates, Blocked);
			AvoidanceDirection = Wanted;
			AvoidanceTimeLeft = AvoidanceCommitTime;
			bClimbCandidate = Wanted.Z > KINDA_SMALL_NUMBER;
		}
	}

	// 고도: 멀리서는 바닥 위 HoverHeight(대상이 더 높으면 대상 높이), 가까이서는 대상 높이로 내려간다.
	// 문턱값이 아니라 오차에 비례하는 연속값이라 흔들리지 않는다. 위로 비켜 가는 후보는 그대로 둔다.
	float WantedVertical = Wanted.Z;
	if (!bClimbCandidate)
	{
		float TargetAltitude = TargetZ;
		float Clearance = Profile->HoverHeight * 3.0f;
		const UWorld* const World = GetWorld();
		if (World)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BeeHover), false, this);
			FHitResult Ground;
			if (World->LineTraceSingleByChannel(Ground, Location, Location - FVector(0.0f, 0.0f, Profile->HoverHeight * 3.0f), ECC_Visibility, Params))
			{
				Clearance = Location.Z - Ground.ImpactPoint.Z - Sphere->GetScaledSphereRadius();
				if (HorizontalDistance > Profile->HoverHeight * 2.0f)
				{
					TargetAltitude = FMath::Max(TargetZ, Ground.ImpactPoint.Z + Profile->HoverHeight);
				}
			}
			else if (HorizontalDistance > Profile->HoverHeight * 2.0f)
			{
				TargetAltitude = FMath::Max(TargetZ, Location.Z);
			}
		}
		WantedVertical = FBeeSteering::VerticalComponent(TargetAltitude - Location.Z, Profile->HoverHeight);
		// 바닥이 가까울수록 얕게 내려간다. 회전이 따라잡기 전에 바닥에 닿지 않게.
		WantedVertical = FMath::Max(WantedVertical, -FBeeSteering::MaxDescent(Clearance, Profile->HoverHeight));
	}

	const FVector NewDirection = FBeeSteering::TurnTowardsSplit(Current, Wanted, WantedVertical, Profile->TurnRateDeg * DeltaTime);
	Movement->Velocity = NewDirection * Profile->Speed;
}

void ABeeProjectile::DropTrail()
{
	UWorld* const World = GetWorld();
	if (!World || !Profile->TrailDeposit.CanPaint())
	{
		return;
	}

	const FVector Now = GetActorLocation();
	FVector Step = Now - LastMark;
	Step.Z = 0.0f;
	const float Spacing = FMath::Max(Profile->MarkSpacing, 5.0f);
	int32 Guard = 8;
	while (Step.SizeSquared() >= FMath::Square(Spacing) && Guard-- > 0)
	{
		LastMark += Step.GetSafeNormal() * Spacing;
		LastMark.Z = Now.Z;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeeTrail), /*bTraceComplex=*/true, this);
		Params.AddIgnoredActor(GetInstigator());
		Params.bReturnFaceIndex = true;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, LastMark, LastMark - FVector(0.0f, 0.0f, Profile->HoverHeight * 3.0f), ECC_Visibility, Params))
		{
			Profile->TrailDeposit.ApplyHit(World, Hit, FVector(0.0f, 0.0f, -800.0f), GetPaintId(), FMath::Rand());
		}

		Step = Now - LastMark;
		Step.Z = 0.0f;
	}
}

void ABeeProjectile::HandleUnitOverlap(AUnit& Unit)
{
	// 아군은 지나간다. 상대에 닿으면 터진다.
	if (FItemAreaEffect::ShouldAffect(Unit.GetTeam(), GetTeam(), /*bIsInstigator=*/false))
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌이 %s에 적중했다."), *GetNameSafe(GetInstigator()), *GetNameSafe(&Unit));
		// 터지기 전에 기록해야 OnDetonate가 맞은 상대를 안다.
		StruckUnit = &Unit;
		Detonate();
	}
}

void ABeeProjectile::HandleWorldHit(const FHitResult& Hit)
{
	// 페인트탄은 껍질이 받는다. 뿌리 구가 탄에 막힐 일은 없지만, 혹시 그렇더라도 터지지는 않는다.
	if (Cast<APaintProjectile>(Hit.GetActor()))
	{
		return;
	}
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌이 %s(%s)에 부딪혔다. 위치 %s, 법선 %s."), *GetNameSafe(GetInstigator()),
		*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *GetActorLocation().ToCompactString(), *Hit.ImpactNormal.ToCompactString());
	Detonate();
}

void ABeeProjectile::Expire()
{
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌의 수명이 다했다."), *GetNameSafe(GetInstigator()));
	Detonate();
}

void ABeeProjectile::ReceivePaintHit_Implementation(float HitPower, uint8 PaintId, const FHitResult& Hit)
{
	if (!HasAuthority() || bDetonated || !Profile || HitPower <= 0.0f)
	{
		return;
	}
	// 자기 팀 탄은 무시한다. 타격에 쏜 사람이 실려 오지 않아 "사용자 외"를 "상대 팀"으로 좁힌다.
	if (PaintId == GetPaintId())
	{
		return;
	}

	Damage += HitPower;
	if (Damage >= Profile->Health)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌이 격추됐다."), *GetNameSafe(GetInstigator()));
		bDetonated = true;
		Destroy();
	}
}

void ABeeProjectile::OnDetonate()
{
	UWorld* const World = GetWorld();
	if (!World || !Profile)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(LifeTimer);

	const FVector Origin = GetActorLocation();
	FPaintBurstParams Burst = Profile->Burst;
	Burst.PaintId = GetPaintId();
	Burst.Seed = FMath::Rand();
	APaintBurst::Spawn(*World, Origin, Burst);

	FItemAreaEffect::Apply(*World, Origin, Profile->StunRadius, GetTeam(), GetInstigatorUnit(), /*bKnockback=*/false);

	// 상대에게 맞은 경우에만, 그 상대의 몸 가운데에서. 발사체는 곧 파괴되므로 자기 멀티캐스트는
	// 도달을 믿을 수 없다. 살아남는 맞은 쪽의 슬롯을 통해 뿌린다.
	if (AUnit* const Struck = StruckUnit.Get())
	{
		if (Profile->HitFX)
		{
			if (UItemSlotComponent* const Slot = Struck->GetItemSlot())
			{
				Slot->MulticastPlayFXAt(Profile->HitFX, Struck->GetActorLocation(), Profile->HitFXScale);
			}
		}
	}

	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌이 터졌다."), *GetNameSafe(GetInstigator()));
}
