#include "Weapons/PaintballProfile.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "Paint/PaintLog.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

void UPaintballProfile::LogUnsetReferences(const UObject* Owner) const
{
	UE_CLOG(!ProjectileClass, LogPaint, Warning, TEXT("%s: %s has no ProjectileClass, it cannot be fired."),
		*GetNameSafe(Owner), *GetName());
	UE_CLOG(!Deposit.CanPaint(), LogPaint, Warning, TEXT("%s: %s has no BrushProfile, its hits will not paint."),
		*GetNameSafe(Owner), *GetName());
}

APaintProjectile* UPaintballProfile::Launch(UWorld& World, const FTransform& SpawnTransform, APawn* Instigator,
	const FVector& Velocity, uint8 PaintId, int32 Seed, bool bCosmetic) const
{
	if (!ProjectileClass)
	{
		return nullptr;
	}

	// 게임의 모든 페인트볼이 이 함수를 지나간다(총, 버스트, 페인트 레인, 풍선). 그래서 풀
	// 연결도 여기 한 곳이면 된다. 풀이 비어 있으면 알아서 새로 스폰하므로 사격은 끊기지 않는다.
	if (UProjectilePoolSubsystem* const Pool = UProjectilePoolSubsystem::Get(&World))
	{
		return Pool->Launch(ProjectileClass, SpawnTransform, Instigator, this, PaintId, Seed, Velocity, bCosmetic);
	}

	// 풀이 없는 월드(테스트 등)에서는 예전처럼 직접 스폰한다.
	// 지연 스폰이어야 무브먼트 컴포넌트가 초기화 때 속도를 읽는다.
	APaintProjectile* const Projectile = World.SpawnActorDeferred<APaintProjectile>(
		ProjectileClass, SpawnTransform, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->Init(this, PaintId, Seed, Velocity, bCosmetic);
	Projectile->FinishSpawning(SpawnTransform);
	return Projectile;
}
