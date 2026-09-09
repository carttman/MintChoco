#include "Items/BeeProjectile.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SphereComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

#include "Game/Unit.h"
#include "Items/BeeProfile.h"
#include "Items/ItemAreaEffect.h"
#include "MintChoco.h"
#include "Weapons/PaintBurst.h"
#include "Weapons/PaintProjectile.h"

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

// ---------------------------------------------------------------- ABeeProjectile

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

	// 대상이 없으면 직진.
	FVector Desired = Current;
	float DistanceToTarget = TNumericLimits<float>::Max();
	if (Target.IsValid())
	{
		const FVector ToTarget = Target->GetActorLocation() - Location;
		DistanceToTarget = ToTarget.Size();
		Desired = ToTarget.GetSafeNormal();
	}

	// 앞이 막혔으면 비켜 갈 후보를 차례로 본다.
	TArray<FVector> Candidates;
	FBeeSteering::BuildCandidates(Desired, Candidates);
	TArray<bool> Blocked;
	Blocked.Reserve(Candidates.Num());
	FVector Wanted = Candidates[0];
	if (IsBlocked(Candidates[0]))
	{
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
	}

	// 바닥에 너무 가까우면 띄운다. 대상에 달려드는 마지막 구간은 예외.
	const UWorld* const World = GetWorld();
	if (World && DistanceToTarget > Profile->HoverHeight * 1.5f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeeHover), false, this);
		FHitResult Ground;
		if (World->LineTraceSingleByChannel(Ground, Location, Location - FVector(0.0f, 0.0f, Profile->HoverHeight), ECC_Visibility, Params))
		{
			Wanted.Z = FMath::Max(Wanted.Z, 0.35f);
			Wanted.Normalize();
		}
	}

	const FVector NewDirection = FBeeSteering::TurnTowards(Current, Wanted, Profile->TurnRateDeg * DeltaTime);
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
	Detonate();
}

void ABeeProjectile::Expire()
{
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
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 꿀벌이 터졌다."), *GetNameSafe(GetInstigator()));
}
