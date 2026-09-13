#include "Items/HoneyBalloonAbility.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Game/Unit.h"
#include "Items/AimArcPreview.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/HoneyBalloonProjectile.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "MintChoco.h"

namespace
{
	/** 총구처럼, 몸에서 이만큼 앞에서 태어난다(cm). 발밑에서 터지지 않게. */
	constexpr float ThrowOffset = 60.0f;

	/** 예측을 멈추는 시간(초). 맵 밖으로 나간 공을 끝없이 따라가지 않는다. */
	constexpr float AimMaxSimTime = 4.0f;

	/** 초당 예측 횟수. 점은 어차피 거리로 다시 뽑으므로 궤적 모양만 유지되면 된다. */
	constexpr float AimSimFrequency = 15.0f;
}

UGA_HoneyBalloon::UGA_HoneyBalloon()
{
	// 조준 중이라는 상태 하나면 충분하다: 무기를 막는 것도, 좌클릭을 가로채는 것도 이 태그를 본다.
	StateTag = ItemTags::State_Item_Aiming;
	EffectClass = UGE_HoneyBalloon::StaticClass();
}

void UGA_HoneyBalloon::OnAimBegan(AUnit& Unit, const UItemProfile& Profile)
{
	BallRadius = 0.0f;

	const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(&Profile);
	if (Honey && Honey->ProjectileClass)
	{
		// 예측이 진짜 공과 같은 굵기로 쓸어야, 모서리를 스칠 때 미리보기와 실제가 갈리지 않는다.
		if (const AHoneyBalloonProjectile* const Default = Honey->ProjectileClass->GetDefaultObject<AHoneyBalloonProjectile>())
		{
			BallRadius = Default->GetCollisionRadius();
		}
	}
}

void UGA_HoneyBalloon::ComputeThrow(const AUnit& Unit, const UHoneyBalloonProfile& Honey, FVector& OutOrigin, FVector& OutVelocity)
{
	// 눈높이에서 시선 방향으로. 서버의 컨트롤 회전은 무브마다 갱신되므로 소유자가 본 것과 같다.
	const FVector Direction = Unit.GetControlRotation().Vector();
	OutOrigin = Unit.GetPawnViewLocation() + Direction * ThrowOffset;
	OutVelocity = Direction * Honey.ThrowSpeed;
}

AActor* UGA_HoneyBalloon::SpawnPreview(AUnit& Unit, const UItemProfile& Profile)
{
	const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Honey || !Honey->AimPreviewClass || !World)
	{
		return nullptr;
	}

	FActorSpawnParameters Spawn;
	Spawn.Owner = &Unit;
	Spawn.Instigator = &Unit;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 점은 월드 좌표로 놓이므로 액터 자신의 트랜스폼은 쓰이지 않는다.
	AAimArcPreview* const Arc = World->SpawnActor<AAimArcPreview>(Honey->AimPreviewClass, FTransform::Identity, Spawn);
	if (Arc)
	{
		// 첫 프레임을 원점에서 보내지 않도록 바로 한 번 그린다.
		RefreshArc(Unit, *Arc, *Honey);
	}
	return Arc;
}

void UGA_HoneyBalloon::UpdatePreview(AUnit& Unit, AActor& InPreview, float DeltaTime)
{
	if (const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(GetItemProfile()))
	{
		RefreshArc(Unit, InPreview, *Honey);
	}
}

void UGA_HoneyBalloon::RefreshArc(AUnit& Unit, AActor& InPreview, const UHoneyBalloonProfile& Honey) const
{
	AAimArcPreview* const Arc = Cast<AAimArcPreview>(&InPreview);
	UWorld* const World = Unit.GetWorld();
	if (!Arc || !World)
	{
		return;
	}

	FVector Origin;
	FVector Velocity;
	ComputeThrow(Unit, Honey, Origin, Velocity);

	FPredictProjectilePathParams Params;
	Params.StartLocation = Origin;
	Params.LaunchVelocity = Velocity;
	Params.ProjectileRadius = BallRadius;
	Params.bTraceWithCollision = true;
	// 진짜 공을 멈추는 것들과 같은 것들에 막힌다: 지형, 움직이는 물체, 그리고 폰(닿으면 터진다).
	Params.bTraceWithChannel = false;
	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	// 던지는 본인은 공도 무시한다(AItemProjectile::Init).
	Params.ActorsToIgnore.Add(&Unit);
	Params.MaxSimTime = AimMaxSimTime;
	Params.SimFrequency = AimSimFrequency;

	// 0은 "월드 중력을 쓰라"는 뜻이다. 중력을 끈 설정을 그대로 넘기면 오히려 떨어진다.
	const float GravityZ = World->GetGravityZ() * Honey.GravityScale;
	Params.OverrideGravityZ = FMath::IsNearlyZero(GravityZ) ? -UE_KINDA_SMALL_NUMBER : GravityZ;

	FPredictProjectilePathResult Result;
	const bool bHit = UGameplayStatics::PredictProjectilePath(World, Params, Result);

	TArray<FVector> Path;
	Path.Reserve(Result.PathData.Num());
	for (const FPredictProjectilePathPointData& Point : Result.PathData)
	{
		Path.Add(Point.Location);
	}

	Arc->SetArc(Path, bHit, Result.HitResult.ImpactPoint, Result.HitResult.ImpactNormal);
}

void UGA_HoneyBalloon::OnAimConfirmed(AUnit& Unit, const UItemProfile& Profile)
{
	const UHoneyBalloonProfile* const Honey = Cast<UHoneyBalloonProfile>(&Profile);
	UWorld* const World = Unit.GetWorld();
	if (!Honey || !Honey->ProjectileClass || !World || !IsAuthority())
	{
		return;
	}

	FVector Origin;
	FVector Velocity;
	ComputeThrow(Unit, *Honey, Origin, Velocity);
	const FTransform SpawnTransform(Velocity.Rotation(), Origin);

	AHoneyBalloonProjectile* const Balloon = World->SpawnActorDeferred<AHoneyBalloonProjectile>(
		Honey->ProjectileClass, SpawnTransform, &Unit, &Unit, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Balloon)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 꿀풍선을 스폰하지 못했다."), *GetNameSafe(&Unit));
		return;
	}
	Balloon->Init(&Unit, Unit.GetTeam(), Velocity);
	Balloon->SetProfile(Honey);
	Balloon->FinishSpawning(SpawnTransform);
}
