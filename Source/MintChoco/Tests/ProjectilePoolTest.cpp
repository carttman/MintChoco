#include "Misc/AutomationTest.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"

#include "Tests/TestPaintProjectile.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintballProfile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 액터를 스폰해야 하므로 월드가 필요하다. 이 파일의 테스트만 쓰는 임시 월드다. */
	UWorld* MakeTestWorld()
	{
		UWorld* const World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}

		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		return World;
	}

	void DestroyTestWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	UPrimitiveComponent* GetPawnBody(APawn* Pawn)
	{
		return Pawn ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	}
}

/**
 * 페인트볼 풀이 실제로 재사용하는지, 그리고 재사용이 조용히 무언가를 망가뜨리지 않는지.
 *
 * 특히 슈터의 이동 무시 목록을 본다. 공은 총구에서 슈터 안쪽에 태어나므로 서로를 무시하도록
 * 슈터의 콜리전에 항목을 하나 박는데, 그 정리가 원래 EndPlay에 있었다. 풀 반납에는 EndPlay가
 * 없으므로 정리를 놓치면 쏠 때마다 목록이 하나씩 영원히 자란다. 눈에 보이는 증상이 늦게
 * 나타나는 종류라 테스트로 못 박아 둔다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectilePoolTest,
	"MintChoco.Weapons.ProjectilePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectilePoolTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MakeTestWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { DestroyTestWorld(World); };

	UProjectilePoolSubsystem* const Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!TestNotNull(TEXT("풀 서브시스템이 월드에 있다"), Pool))
	{
		return false;
	}

	UPaintballProfile* const Profile = NewObject<UPaintballProfile>();
	APawn* const Shooter = World->SpawnActor<ADefaultPawn>();
	UPrimitiveComponent* const Body = GetPawnBody(Shooter);
	if (!TestNotNull(TEXT("슈터의 콜리전 루트"), Body))
	{
		return false;
	}

	const TSubclassOf<APaintProjectile> Class = ATestPaintProjectile::StaticClass();
	const FVector Velocity(1000.0f, 0.0f, 0.0f);

	// 한 발 쏘고 돌려받기를 반복한다. 풀이 돌면 같은 액터가 계속 돌아와야 한다.
	TSet<APaintProjectile*> Seen;
	constexpr int32 Shots = 200;
	for (int32 Index = 0; Index < Shots; ++Index)
	{
		APaintProjectile* const Ball = Pool->Launch(
			Class, FTransform::Identity, Shooter, Profile, /*PaintId=*/1, /*Seed=*/Index, Velocity, /*bCosmetic=*/true);
		if (!Ball)
		{
			AddError(FString::Printf(TEXT("%d번째 발사가 공을 만들지 못했다"), Index));
			return false;
		}

		Seen.Add(Ball);
		Pool->Release(Ball);
	}

	TestEqual(TEXT("한 발씩 쏘고 돌려받으면 같은 공이 재사용된다"), Seen.Num(), 1);

	// 이 테스트의 핵심. 정리가 빠지면 여기가 200이 된다.
	TestEqual(TEXT("슈터의 이동 무시 목록에 찌꺼기가 남지 않는다"), Body->GetMoveIgnoreActors().Num(), 0);

	// 풀에서 나온 공은 날 수 있는 상태여야 한다.
	APaintProjectile* const Reused = Pool->Launch(
		Class, FTransform::Identity, Shooter, Profile, 1, 0, Velocity, true);
	if (TestNotNull(TEXT("재사용된 공"), Reused))
	{
		TestFalse(TEXT("재사용된 공은 보인다"), Reused->IsHidden());
		TestTrue(TEXT("재사용된 공은 충돌한다"), Reused->GetActorEnableCollision());
		Pool->Release(Reused);
	}

	// 같은 공을 두 번 반납해도 풀에 중복이 생기면 안 된다. 중복되면 한 공이 두 갈래로 날아간다.
	APaintProjectile* const Once = Pool->Launch(Class, FTransform::Identity, Shooter, Profile, 1, 0, Velocity, true);
	Pool->Release(Once);
	Pool->Release(Once);
	APaintProjectile* const A = Pool->Launch(Class, FTransform::Identity, Shooter, Profile, 1, 0, Velocity, true);
	APaintProjectile* const B = Pool->Launch(Class, FTransform::Identity, Shooter, Profile, 1, 0, Velocity, true);
	TestNotEqual(TEXT("두 번 반납해도 같은 공이 두 번 나오지 않는다"), A, B);

	// 풀이 비면 새로 스폰해서 버틴다: 사격이 끊기지 않아야 한다.
	TArray<APaintProjectile*> Burst;
	for (int32 Index = 0; Index < 32; ++Index)
	{
		Burst.Add(Pool->Launch(Class, FTransform::Identity, Shooter, Profile, 1, Index, Velocity, true));
	}
	int32 Spawned = 0;
	for (APaintProjectile* const Ball : Burst)
	{
		Spawned += (Ball != nullptr) ? 1 : 0;
	}
	TestEqual(TEXT("풀이 비어도 32발이 전부 나간다"), Spawned, 32);

	return true;
}

/** 미리 만들어 둔 공은 재워진 채로 풀에 들어가 있어야 한다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectilePoolPrewarmTest,
	"MintChoco.Weapons.ProjectilePoolPrewarm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectilePoolPrewarmTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MakeTestWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { DestroyTestWorld(World); };

	UProjectilePoolSubsystem* const Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!TestNotNull(TEXT("풀 서브시스템"), Pool))
	{
		return false;
	}

	const TSubclassOf<APaintProjectile> Class = ATestPaintProjectile::StaticClass();
	Pool->Prewarm(Class, 8);

	UPaintballProfile* const Profile = NewObject<UPaintballProfile>();
	APawn* const Shooter = World->SpawnActor<ADefaultPawn>();

	// 예열된 공이 그대로 나와야 한다: 여덟 발을 쏘는 동안 새로 만들어질 이유가 없다.
	TSet<APaintProjectile*> Launched;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		if (APaintProjectile* const Ball = Pool->Launch(
			Class, FTransform::Identity, Shooter, Profile, 1, Index, FVector(1000.0f, 0.0f, 0.0f), true))
		{
			Launched.Add(Ball);
			TestFalse(TEXT("예열된 공은 꺼내면 보인다"), Ball->IsHidden());
		}
	}

	TestEqual(TEXT("예열한 여덟 개가 서로 다른 공으로 나온다"), Launched.Num(), 8);
	return true;
}

#endif
