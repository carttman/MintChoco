#include "Weapons/PaintProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "MeshScale.h"
#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Game/TeamLook.h"
#include "Game/TeamTypes.h"
#include "Game/Unit.h"
#include "Paint/PaintSplat.h"
#include "Weapons/PaintAimMath.h"
#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintWeaponComponent.h"
#include "Weapons/PaintballProfile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

namespace
{
	/** Custom Primitive Data slots the body material reads; the contract is documented on APaintProjectile. */
	constexpr int32 BodySpeedIndex = 0;
	constexpr int32 BodyPhaseIndex = 1;
	constexpr int32 BodyBirthTimeIndex = 2;

	UPrimitiveComponent* GetMovingBody(const APawn* Pawn)
	{
		return Pawn ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	}
}

APaintProjectile::APaintProjectile()
{
	// A plain ball needs no tick of its own. Only a ball that paints as it flies turns it on, in
	// Init, once the profile is known; bCanEverTick has to be set here for that to be possible.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	InitialLifeSpan = PaintballLifeSpanSeconds;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(6.0f);
	// Blocks paintable meshes and hit receivers, but only *overlaps* pawns: a ball still dies and
	// reports the contact the moment it touches a pawn, yet it never blocks or shoves the pawn (a
	// blocking ball is a body the character movement depenetrates from). Query only, no physics body,
	// for the same reason. The camera probe is excused so a ball never shoves the spring arm, and
	// other balls are excused because every pellet of a shot leaves the same muzzle point:
	// overlapping at birth, they would otherwise hit each other on their first move and die before flying.
	Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(PaintballChannel);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(PaintballChannel, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->OnComponentHit.AddDynamic(this, &APaintProjectile::OnHit);
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &APaintProjectile::OnPawnOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The body material stretches a tail behind the sphere, past the mesh's own bounds.
	Mesh->BoundsScale = 2.0f;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bInitialVelocityInLocalSpace = false;
}

void APaintProjectile::Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity, bool bInCosmetic,
	float InDropAfterOverride, const FVector& InVisualOffset)
{
	check(InProfile);
	Profile = InProfile;
	PaintId = InPaintId;
	Seed = InSeed;
	bCosmetic = bInCosmetic;

	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = 0.0f;
	Movement->Velocity = Velocity;
	Movement->ProjectileGravityScale = Profile->GravityScale;

	// 사거리를 보이지 않는 선에서 지우는 대신, 날던 공이 힘을 잃고 떨어지는 것으로 보여준다.
	const float DropAfter = InDropAfterOverride >= 0.0f ? InDropAfterOverride : Profile->DropAfter;
	if (DropAfter > 0.0f)
	{
		GetWorldTimerManager().SetTimer(DropTimer, this, &APaintProjectile::ApplyDropGravity, DropAfter, /*bLoop=*/false);
	}
	else if (InDropAfterOverride == 0.0f)
	{
		// 0 은 “처음부터 떨어져라” 다. 타이머로는 표현할 수 없으므로 그 자리에서 무겁게 만든다.
		ApplyDropGravity();
	}

	Sphere->SetSphereRadius(Profile->Radius);
	ScaleMeshToRadius(Mesh, Profile->Radius);

	if (UMaterialInterface* const TeamMaterial = GetTeamMaterial(PaintId))
	{
		Mesh->SetMaterial(0, TeamMaterial);
	}
	// Per-ball values ride Custom Primitive Data so every ball of a team shares one material. The
	// phase comes from the seed, so a client's cosmetic ball wobbles in step with the server's.
	Mesh->SetCustomPrimitiveDataFloat(BodySpeedIndex, Movement->InitialSpeed);
	Mesh->SetCustomPrimitiveDataFloat(BodyPhaseIndex, FRandomStream(Seed).GetFraction());
	Mesh->SetCustomPrimitiveDataFloat(BodyBirthTimeIndex, GetWorld()->GetTimeSeconds());

	// A ball leaves the muzzle inside the shooter's reach; neither body may collide with the other.
	if (APawn* const Shooter = GetInstigator())
	{
		Sphere->IgnoreActorWhenMoving(Shooter, true);
		if (UPrimitiveComponent* const Body = GetMovingBody(Shooter))
		{
			Body->IgnoreActorWhenMoving(this, true);
		}
	}

	// Splats reach the other machines through the game state's log, so only the server's real ball
	// may paint. A cosmetic ball ticking here would draw the same trail a second time.
	bPaintsTrail = !bCosmetic && HasAuthority() && Profile->HasTrail();
	LastTrailLocation = GetActorLocation();

	// The mesh starts where the gun is and slides onto the path the physics flies, so the ball
	// leaves the barrel on screen while the trajectory belongs to the crosshair.
	VisualOffset = Profile->VisualMergeSeconds > 0.0f ? InVisualOffset : FVector::ZeroVector;
	MergeElapsed = 0.0f;
	bMerging = !VisualOffset.IsNearlyZero();
	Mesh->SetWorldLocation(GetActorLocation() + VisualOffset);

	SetActorTickEnabled(bPaintsTrail || bMerging);
}

UMaterialInterface* APaintProjectile::GetTeamMaterial(uint8 InPaintId) const
{
	return TeamMaterials.IsValidIndex(InPaintId) ? TeamMaterials[InPaintId].Get() : nullptr;
}

void APaintProjectile::RestoreForReuse()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 충돌로 멈춘 무브먼트는 UpdatedComponent를 놓아 버린 상태다. 다시 쥐여 주지 않으면
	// 속도를 넣어도 움직이지 않는다.
	if (Movement->UpdatedComponent != Sphere)
	{
		Movement->SetUpdatedComponent(Sphere);
	}
	Movement->Activate(/*bReset=*/true);

	SetLifeSpan(InitialLifeSpan);
}

void APaintProjectile::Deactivate()
{
	// 쏜 사람의 무시 목록에서 이 공을 뺀다. 풀 반납에는 EndPlay가 없으므로 여기가 유일한 기회다.
	if (UPrimitiveComponent* const Body = GetMovingBody(GetInstigator()))
	{
		Body->IgnoreActorWhenMoving(this, false);
	}
	Sphere->ClearMoveIgnoreActors();

	// 2단 중력 타이머는 이 공에 물려 있다. 풀로 자러 가는 공에 남겨 두면 다음에 꺼내 쓴
	// 공에서 뒤늦게 터져 남의 탄도를 꺾는다(풀링과 DropAfter가 합쳐지며 생긴 자리다).
	GetWorldTimerManager().ClearTimer(DropTimer);

	// 합류 중이던 메시를 제자리로. 다음에 꺼내 쓴 공이 남의 총구 오프셋을 물려받지 않게.
	bMerging = false;
	VisualOffset = FVector::ZeroVector;
	Mesh->SetRelativeLocation(FVector::ZeroVector);

	Movement->StopMovementImmediately();
	Movement->Deactivate();

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.0f);
}

void APaintProjectile::LifeSpanExpired()
{
	if (UProjectilePoolSubsystem* const Pool = UProjectilePoolSubsystem::Get(this))
	{
		Pool->Release(this);
		return;
	}
	Super::LifeSpanExpired();
}

void APaintProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	Deactivate();
	Super::EndPlay(Reason);
}

void APaintProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMerging)
	{
		MergeElapsed += DeltaSeconds;
		const float Alpha = PaintAim::MergeAlpha(MergeElapsed, Profile ? Profile->VisualMergeSeconds : 0.0f);
		Mesh->SetWorldLocation(GetActorLocation() + VisualOffset * (1.0f - Alpha));
		if (Alpha >= 1.0f)
		{
			bMerging = false;
			Mesh->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	if (!bPaintsTrail || !Profile || TrailSplatCount >= Profile->MaxTrailSplats)
	{
		bPaintsTrail = false;
		SetActorTickEnabled(bMerging);
		return;
	}

	// Measured along the path actually flown rather than from the muzzle, so a lobbed arc is
	// sampled at even spacing along its curve instead of bunching up near the apex.
	const FVector Location = GetActorLocation();
	TrailDistance += FVector::Dist(LastTrailLocation, Location);
	LastTrailLocation = Location;
	if (TrailDistance < Profile->TrailSpacing)
	{
		return;
	}

	// One sample per frame at most: a ball moving several spacings in one frame would otherwise
	// fire a burst of traces to catch up, which is exactly the cost the spacing exists to bound.
	TrailDistance -= Profile->TrailSpacing;
	TrailSplatCount += PaintTrailSample(Location, TrailSampleCount++);
}

void APaintProjectile::ApplyDropGravity()
{
	if (Movement && Profile)
	{
		// 컴포넌트가 매 프레임 읽는 값이라, 바꾸는 순간부터 다음 프레임에 바로 적용된다.
		Movement->ProjectileGravityScale = Profile->DropGravityScale;
	}
}

int32 APaintProjectile::PaintTrailSample(const FVector& Location, int32 SampleIndex)
{
	UWorld* const World = GetWorld();
	const FVector Forward = Movement->Velocity.GetSafeNormal();
	if (!World || Forward.IsNearlyZero())
	{
		return 0;
	}

	// The rays lie in the plane across the flight direction, so one ray count covers floor,
	// ceiling and both sides however the ball is heading - no special case for "below".
	FVector RayUp;
	FVector RaySide;
	Forward.FindBestAxisVectors(RayUp, RaySide);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintProjectileTrail), /*bTraceComplex=*/false, this);
	Params.AddIgnoredActor(GetInstigator());

	const int32 Rays = Profile->TrailRayCount;
	const int32 Budget = Profile->MaxTrailSplats - TrailSplatCount;
	// Turning the fan by a per-sample amount keeps successive samples from lining their rays up
	// into a visible ladder. It comes from the seed, so a replay lands the marks in the same places.
	const float Twist = 2.0f * UE_PI * static_cast<float>(HashCombineFast(static_cast<uint32>(Seed), static_cast<uint32>(SampleIndex)) & 0xFFFF) / 65536.0f;

	int32 Painted = 0;
	for (int32 Index = 0; Index < Rays && Painted < Budget; ++Index)
	{
		const float Angle = Twist + 2.0f * UE_PI * static_cast<float>(Index) / static_cast<float>(Rays);
		const FVector Direction = RayUp * FMath::Cos(Angle) + RaySide * FMath::Sin(Angle);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Location, Location + Direction * Profile->TrailRadius, PaintballChannel, Params)
			|| Hit.bStartPenetrating)
		{
			continue;
		}

		// A pawn is never painted by a trail: the mark would not stick to it, and ApplyHit would
		// also strike it as a paint hit receiver once per sample.
		if (Cast<APawn>(Hit.GetActor()))
		{
			continue;
		}

		if (Profile->bTrailSkipTransient && !FPaintDeposit::KeepsPaint(Hit))
		{
			continue;
		}

		// The incident velocity runs along the ray, so the brush stretches the stamp away from the
		// path and the samples read as one stripe rather than a row of dots.
		const int32 RaySeed = static_cast<int32>(HashCombineFast(
			static_cast<uint32>(Seed), static_cast<uint32>(SampleIndex * Rays + Index + 1)));
		if (Profile->TrailDeposit.ApplyHit(World, Hit, Direction * Movement->Velocity.Size(), PaintId, RaySeed))
		{
			++Painted;
		}
	}
	return Painted;
}

void APaintProjectile::OnPawnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only pawns overlap; everything else blocks and arrives through OnHit. The shooter is already
	// on the ignore list, so this is another pawn. A sweep result carries the impact point; a
	// non-sweep overlap (the pawn walked into a ball) uses the ball itself as the contact.
	if (!Cast<APawn>(OtherActor))
	{
		return;
	}

	// A ball of the pawn's own colour passes through: the burst of an item lands on the user who
	// fired it (and on teammates), and those balls have no instigator to be excused by the ignore list.
	if (const AUnit* const Unit = Cast<AUnit>(OtherActor))
	{
		if (Unit->GetPaintId() == PaintId)
		{
			return;
		}
	}

	FHitResult Hit = SweepResult;
	if (!bFromSweep)
	{
		Hit.ImpactPoint = GetActorLocation();
		Hit.Location = GetActorLocation();
		Hit.ImpactNormal = -Movement->Velocity.GetSafeNormal();
		Hit.Normal = Hit.ImpactNormal;
		Hit.HitObjectHandle = FActorInstanceHandle(OtherActor);
	}
	OnHit(nullptr, OtherActor, nullptr, FVector::ZeroVector, Hit);
}

void APaintProjectile::OnHit(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	// 풀에서 자고 있는 공에는 콜리전이 없지만, 반납 직전에 큐에 들어간 이벤트가 뒤늦게
	// 도착할 수 있다. 숨어 있으면 이미 반납된 공이다.
	if (!IsValid(this) || IsActorBeingDestroyed() || IsHidden())
	{
		return;
	}
	if (Profile && !bCosmetic)
	{
		// The hit fires from inside the move, before the movement component zeroes its velocity,
		// so this is still the impact velocity; the fallback covers a blocked first step.
		FVector Velocity = Movement->Velocity;
		if (Velocity.IsNearlyZero())
		{
			Velocity = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal() * Movement->InitialSpeed;
		}
		Profile->Deposit.ApplyHit(GetWorld(), Hit, Velocity, PaintId, Seed);
	}

	// 연출용 공도 그린다: 착탄은 각 머신에서 제 공으로 일어나므로 이것이 그 화면의 한 번이다.
	// 소리도 같은 이유로 여기서 한 번. 한 발의 산탄이 한꺼번에 닿으므로 뱅크의 동시발성 제한이 자른다.
	UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_Weapon_Impact, Hit.ImpactPoint);
	if (Profile && Profile->ImpactFX)
	{
		UWorld* const World = GetWorld();
		if (World && World->GetNetMode() != NM_DedicatedServer)
		{
			// MakeFromZ다. FVector::Rotation()은 넘긴 방향을 +X(앞)로 삼으므로 바닥 법선을 주면
			// 이펙트가 90도 눕는다. 이펙트의 위쪽인 +Z를 법선에 맞춰야 바닥에 선 채로 나온다.
			const FRotator Upright = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator();
			if (UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, Profile->ImpactFX, Hit.ImpactPoint, Upright, FVector(Profile->ImpactFXScale)))
			{
				FX->SetVariableLinearColor(TeamLook::NiagaraTintParameter, TeamLook::GetColor(PaintId, World));
			}
		}
	}

	if (UProjectilePoolSubsystem* const Pool = UProjectilePoolSubsystem::Get(this))
	{
		Pool->Release(this);
		return;
	}
	Destroy();
}
