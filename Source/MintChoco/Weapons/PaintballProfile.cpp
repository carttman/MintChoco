#include "Weapons/PaintballProfile.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Game/TeamLook.h"
#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintSplashProfile.h"
#include "Paint/PaintSplashSubsystem.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

void UPaintballProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!ProjectileClass, LogPaint, Warning, TEXT("%s: %s has no ProjectileClass, it cannot be fired."),
		*GetNameSafe(Owner), *GetName());
	UE_CLOG(!Deposit.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no BrushProfile, its hits will not paint."),
		*GetNameSafe(Owner), *GetName());
	UE_CLOG(Deposit.Splash && (!Deposit.Splash->DropletBrush || !Deposit.Splash->DropletBrush->BrushMaterial), LogPaint, Warning,
		TEXT("%s: %s splashes with no DropletBrush material, so its droplets neither mark nor score."), *GetNameSafe(Owner), *GetName());
	UE_CLOG(Deposit.Splash && !ImpactFX, LogPaint, Warning,
		TEXT("%s: %s splashes but has no ImpactFX, so nothing flies and nothing marks; only the score sees the splash."), *GetNameSafe(Owner), *GetName());
}

void UPaintballProfile::PlayImpactEffect(UWorld& World, const FHitResult& Hit, const FVector& IncidentVelocity, uint8 PaintId, int32 Seed) const
{
	if (!ImpactFX || World.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// The splash is this machine's own: its droplets and their marks are drawn here from the same
	// contact, cosmetic ball or not, and only the phantom landings inside the replicated splat
	// count. Booked before the effect spawns so the effect can be told where to report landings.
	FPaintSplashRequest Splash;
	Splash.Profile = Deposit.Splash;
	Splash.ImpactPoint = Hit.ImpactPoint;
	Splash.ImpactNormal = Hit.ImpactNormal;
	Splash.IncidentVelocity = IncidentVelocity;
	Splash.BallRadius = Radius;
	Splash.PaintId = PaintId;
	Splash.Seed = Seed;
	Splash.bLeavesMarks = FPaintDeposit::ReceivesSplat(Hit);
	UPaintSplashSubsystem* const SplashSystem = Splash.Profile ? UPaintSplashSubsystem::Get(&World) : nullptr;
	UPaintSplashLandingHandler* const Handler = SplashSystem ? SplashSystem->BeginSplash(Splash) : nullptr;

	// MakeFromZ다. FVector::Rotation()은 넘긴 방향을 +X(앞)로 삼으므로 바닥 법선을 주면
	// 이펙트가 90도 눕는다. 이펙트의 위쪽인 +Z를 법선에 맞춰야 바닥에 선 채로 나온다.
	// 풀에서 돌려 쓴다: 착탄은 초당 수십 번이고 컴포넌트 생성이 그 프레임에 그대로 얹힌다.
	const FRotator Upright = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator();
	UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		&World, ImpactFX, Hit.ImpactPoint + Hit.ImpactNormal, Upright, FVector(ImpactFXScale),
		/*bAutoDestroy=*/true, /*bAutoActivate=*/true, ENCPoolMethod::AutoRelease);
	if (!FX)
	{
		return;
	}
	FX->SetVariableLinearColor(TeamLook::NiagaraTintParameter, TeamLook::GetColor(PaintId, &World));
	FX->SetVariableFloat(TeamLook::NiagaraTeamIdParameter, static_cast<float>(PaintId));
	if (SplashSystem)
	{
		SplashSystem->ConfigureEffect(*FX, Splash, Handler);
	}
}

APaintProjectile* UPaintballProfile::Launch(UWorld& World, const FTransform& SpawnTransform, APawn* Instigator,
	const FVector& Velocity, uint8 PaintId, int32 Seed, bool bCosmetic, float DropAfterOverride,
	const FVector& VisualOffset) const
{
	if (!ProjectileClass)
	{
		return nullptr;
	}

	// 게임의 모든 페인트볼이 이 함수를 지나간다(총, 버스트, 페인트 레인, 풍선). 그래서 풀
	// 연결도 여기 한 곳이면 된다. 풀이 비어 있으면 알아서 새로 스폰하므로 사격은 끊기지 않는다.
	if (UProjectilePoolSubsystem* const Pool = UProjectilePoolSubsystem::Get(&World))
	{
		return Pool->Launch(ProjectileClass, SpawnTransform, Instigator, this, PaintId, Seed, Velocity,
			bCosmetic, DropAfterOverride, VisualOffset);
	}

	// 풀이 없는 월드(테스트 등)에서는 예전처럼 직접 스폰한다.
	// 지연 스폰이어야 무브먼트 컴포넌트가 초기화 때 속도를 읽는다.
	APaintProjectile* const Projectile = World.SpawnActorDeferred<APaintProjectile>(
		ProjectileClass, SpawnTransform, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->Init(this, PaintId, Seed, Velocity, bCosmetic, DropAfterOverride, VisualOffset);
	Projectile->FinishSpawning(SpawnTransform);
	return Projectile;
}
