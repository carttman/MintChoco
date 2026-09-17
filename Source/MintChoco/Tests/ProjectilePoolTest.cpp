#include "Misc/AutomationTest.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"

#include "Tests/TestPaintProjectile.h"
#include "Tests/TestWorld.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintballProfile.h"
#include "Weapons/ProjectilePoolSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

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
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

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


/**
 * 재활용한 공은 깨어나는 순간부터 이번 사격의 공이어야 한다.
 *
 * 풀에서 꺼낸 공은 총구에서, 즉 쏜 사람의 몸 안에서 콜리전이 켜지고, 엔진은 그 순간 그 자리의
 * 초기 오버랩을 곧바로 질의한다. 그 앞에 Init이 끝나 있지 않으면 그 판정은 지난 사격의
 * PaintId·Profile과 빈 무시 목록으로 이뤄져, 상대 색을 칠하던 공이 이번에 쏜 사람을 기절시키고
 * (히어로 랜딩, 스위트 스피너) 제 팀 초코돔이 제 팀 버스트를 삼킨다(초콜릿 분수의 미도색).
 * 한 색만 도는 풀에서는 지난 값도 늘 같은 색이라 아무 일이 없다: 멀티에서, 두 색이 섞인
 * 뒤부터만 나오는 증상이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectilePoolReuseIdentityTest,
	"MintChoco.Weapons.ProjectilePoolReuseIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProjectilePoolReuseIdentityTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	UProjectilePoolSubsystem* const Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!TestNotNull(TEXT("풀 서브시스템"), Pool))
	{
		return false;
	}

	UPaintballProfile* const Profile = NewObject<UPaintballProfile>();
	APawn* const Shooter = World->SpawnActor<ADefaultPawn>();
	const TSubclassOf<APaintProjectile> Class = ATestPaintProjectile::StaticClass();
	const FVector Velocity(1000.0f, 0.0f, 0.0f);
	constexpr uint8 Mint = 0;
	constexpr uint8 Choco = 1;

	// 상대 색으로 한 발 쏘고 돌려받는다. 풀에 남는 것은 초코 색을 기억하는 공이다.
	APaintProjectile* const ChocoBall = Pool->Launch(
		Class, FTransform::Identity, nullptr, Profile, Choco, /*Seed=*/1, Velocity, /*bCosmetic=*/false);
	if (!TestNotNull(TEXT("초코 탄"), ChocoBall))
	{
		return false;
	}

	UTestCollisionProbeComponent* const Probe = NewObject<UTestCollisionProbeComponent>(ChocoBall);
	Probe->RegisterComponent();
	Pool->Release(ChocoBall);

	// 반납하며 콜리전이 꺼진 것까지가 지난 사격이다. 여기서부터 이번 사격을 본다.
	Probe->PaintIdWhenCollisionChanged.Reset();
	Probe->MoveIgnoreCountWhenCollisionChanged.Reset();

	APaintProjectile* const MintBall = Pool->Launch(
		Class, FTransform::Identity, Shooter, Profile, Mint, /*Seed=*/2, Velocity, /*bCosmetic=*/false);
	TestEqual(TEXT("반납한 그 공이 다시 나온다"), MintBall, ChocoBall);

	// 대조군. 이 줄이 실패하면 아래 검사는 아무것도 지키지 못한다.
	if (!TestTrue(TEXT("재사용은 콜리전을 다시 켠다"), Probe->PaintIdWhenCollisionChanged.Num() > 0))
	{
		return false;
	}

	for (const uint8 PaintIdAtWake : Probe->PaintIdWhenCollisionChanged)
	{
		TestEqual(TEXT("콜리전이 켜지는 순간 이미 이번 사격의 색이다"),
			static_cast<int32>(PaintIdAtWake), static_cast<int32>(Mint));
	}

	// 무시 목록도 같은 순간에 서 있어야 쏜 사람이 제 탄에 맞지 않는다.
	for (const int32 IgnoreCount : Probe->MoveIgnoreCountWhenCollisionChanged)
	{
		TestTrue(TEXT("콜리전이 켜지는 순간 쏜 사람은 이미 무시 목록에 있다"), IgnoreCount > 0);
	}

	return true;
}

#endif
