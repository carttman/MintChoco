#include "Weapons/ProjectilePoolSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "MintChoco.h"
#include "Weapons/PaintProjectile.h"

UProjectilePoolSubsystem* UProjectilePoolSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
}

void UProjectilePoolSubsystem::Deinitialize()
{
	if (PeakLiveCount > 0)
	{
		UE_LOG(LogMintChoco, Verbose, TEXT("페인트볼 풀: 동시 최대 %d개."), PeakLiveCount);
	}

	FreeLists.Empty();
	Super::Deinitialize();
}

APaintProjectile* UProjectilePoolSubsystem::AcquireIdle(
	TSubclassOf<APaintProjectile> Class, const FTransform& Where, APawn* Instigator, bool& bOutFresh)
{
	UWorld* const World = GetWorld();
	if (!World || !Class)
	{
		bOutFresh = false;
		return nullptr;
	}

	if (FProjectileFreeList* const List = FreeLists.Find(Class))
	{
		// 밖에서 파괴된 공이 섞여 있을 수 있다(초코돔이 상대 탄을 삼킬 때 Destroy를 부른다).
		while (List->Projectiles.Num() > 0)
		{
			APaintProjectile* const Recycled = List->Projectiles.Pop(EAllowShrinking::No).Get();
			if (!IsValid(Recycled))
			{
				continue;
			}

			bOutFresh = false;
			Recycled->SetActorTransform(Where, /*bSweep=*/false, nullptr, ETeleportType::ResetPhysics);
			Recycled->SetOwner(Instigator);
			Recycled->SetInstigator(Instigator);
			Recycled->RestoreForReuse();
			return Recycled;
		}
	}

	// 풀이 비었으면 그냥 새로 만든다. 지연 스폰이어야 무브먼트가 초기화 때 속도를 읽는다.
	bOutFresh = true;
	return World->SpawnActorDeferred<APaintProjectile>(
		Class, Where, Instigator, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
}

APaintProjectile* UProjectilePoolSubsystem::Launch(
	TSubclassOf<APaintProjectile> Class,
	const FTransform& Where,
	APawn* Instigator,
	const UPaintballProfile* Profile,
	uint8 PaintId,
	int32 Seed,
	const FVector& Velocity,
	bool bCosmetic)
{
	bool bFresh = false;
	APaintProjectile* const Projectile = AcquireIdle(Class, Where, Instigator, bFresh);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->Init(Profile, PaintId, Seed, Velocity, bCosmetic);

	if (bFresh)
	{
		Projectile->FinishSpawning(Where);
	}

	++LiveCount;
	PeakLiveCount = FMath::Max(PeakLiveCount, LiveCount);
	return Projectile;
}

void UProjectilePoolSubsystem::Release(APaintProjectile* Projectile)
{
	if (!IsValid(Projectile))
	{
		return;
	}

	Projectile->Deactivate();

	FProjectileFreeList& List = FreeLists.FindOrAdd(Projectile->GetClass());

	// 같은 공이 두 번 반납되면(피격과 수명 만료가 같은 프레임에 겹치는 경우) 풀에 중복이
	// 생기고 한 공이 두 번 날아간다. 값이 싸므로 그냥 확인한다.
	if (List.Projectiles.Contains(Projectile))
	{
		return;
	}

	List.Projectiles.Add(Projectile);
	LiveCount = FMath::Max(0, LiveCount - 1);
}

void UProjectilePoolSubsystem::Prewarm(TSubclassOf<APaintProjectile> Class, int32 Count)
{
	UWorld* const World = GetWorld();
	if (!World || !Class || Count <= 0)
	{
		return;
	}

	FProjectileFreeList& List = FreeLists.FindOrAdd(Class);
	List.Projectiles.Reserve(List.Projectiles.Num() + Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APaintProjectile* const Projectile = World->SpawnActor<APaintProjectile>(Class, FTransform::Identity, Params);
		if (!Projectile)
		{
			break;
		}

		Projectile->Deactivate();
		List.Projectiles.Add(Projectile);
	}

	UE_LOG(LogMintChoco, Verbose, TEXT("페인트볼 풀 예열: %s %d개."), *GetNameSafe(Class), List.Projectiles.Num());
}
