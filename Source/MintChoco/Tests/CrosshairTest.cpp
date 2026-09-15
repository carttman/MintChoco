#include "Misc/AutomationTest.h"

#include "Engine/World.h"

#include "Tests/TestWorld.h"
#include "Weapons/PaintBracketCrosshairWidget.h"
#include "Weapons/PaintCrosshairHostWidget.h"
#include "Weapons/PaintCrosshairWidget.h"
#include "Weapons/PaintGunProfile.h"
#include "Weapons/PaintScopeCrosshairWidget.h"
#include "Weapons/PaintScatterProfile.h"
#include "Weapons/PaintSniperProfile.h"
#include "Weapons/PaintballProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 크로스헤어의 순수 계산: 마커 조건은 "조준점보다 앞에 떨어진다", 중앙 원의 크기·알파 블렌드. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCrosshairMathTest,
	"MintChoco.Weapons.Crosshair.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCrosshairMathTest::RunTest(const FString& Parameters)
{
	const FVector Origin(0.0f, 0.0f, 150.0f);
	const FVector Forward = FVector::ForwardVector;
	const FVector Aim(1000.0f, 0.0f, 150.0f);

	TestFalse(TEXT("the same point never falls short"), FPaintCrosshairMath::ImpactFallsShort(Origin, Forward, Aim, Aim, 50.0f));
	TestFalse(TEXT("inside the threshold is not short"), FPaintCrosshairMath::ImpactFallsShort(Origin, Forward, Aim, FVector(960.0f, 0.0f, 120.0f), 50.0f));
	TestTrue(TEXT("dropping 60 cm before the aim point is short"), FPaintCrosshairMath::ImpactFallsShort(Origin, Forward, Aim, FVector(940.0f, 0.0f, 100.0f), 50.0f));
	TestFalse(TEXT("landing past the aim point is not short"), FPaintCrosshairMath::ImpactFallsShort(Origin, Forward, Aim, FVector(1100.0f, 0.0f, 150.0f), 50.0f));
	// 깊이는 시선 방향 성분만 본다: 옆으로 벗어나도 앞뒤가 같으면 같다.
	TestFalse(TEXT("sideways offset alone is not short"), FPaintCrosshairMath::ImpactFallsShort(Origin, Forward, Aim, FVector(1000.0f, 300.0f, 150.0f), 50.0f));

	TestEqual(TEXT("idle radius is the base"), FPaintCrosshairMath::CenterRadius(8.0f, 1.3f, 0.0f), 8.0f, 1e-4f);
	TestEqual(TEXT("firing radius is scaled"), FPaintCrosshairMath::CenterRadius(8.0f, 1.3f, 1.0f), 10.4f, 1e-4f);
	TestEqual(TEXT("idle opacity"), FPaintCrosshairMath::CenterOpacity(0.6f, 1.0f, 0.0f, 0.0f, 0.35f), 0.6f, 1e-4f);
	TestEqual(TEXT("firing opacity"), FPaintCrosshairMath::CenterOpacity(0.6f, 1.0f, 1.0f, 0.0f, 0.35f), 1.0f, 1e-4f);
	TestEqual(TEXT("a visible marker dims the center"), FPaintCrosshairMath::CenterOpacity(0.6f, 1.0f, 1.0f, 1.0f, 0.35f), 0.35f, 1e-4f);
	return true;
}

/** 어느 크로스헤어를 띄울지: 프로필이 고른 클래스, 없으면 호스트 기본. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCrosshairResolveTest,
	"MintChoco.Weapons.Crosshair.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCrosshairResolveTest::RunTest(const FString& Parameters)
{
	const TSubclassOf<UPaintCrosshairWidget> Default = UPaintBracketCrosshairWidget::StaticClass();

	TestTrue(TEXT("no profile falls back to the default"),
		UPaintCrosshairHostWidget::ResolveCrosshairClass(nullptr, Default) == Default);

	UPaintGunProfile* const Gun = NewObject<UPaintGunProfile>();
	TestTrue(TEXT("an unset class falls back to the default"),
		UPaintCrosshairHostWidget::ResolveCrosshairClass(Gun, Default) == Default);

	Gun->CrosshairClass = UPaintScopeCrosshairWidget::StaticClass();
	TestTrue(TEXT("the profile's choice wins"),
		UPaintCrosshairHostWidget::ResolveCrosshairClass(Gun, Default) == UPaintScopeCrosshairWidget::StaticClass());
	return true;
}

namespace
{
	/** 총·산포·공을 코드에서 조립한 건 프로필. 에셋 없이 예측 경로만 본다. */
	UPaintGunProfile* MakeGun(float MuzzleSpeed, float GravityScale, float DropAfter, float DropGravityScale)
	{
		UPaintGunProfile* const Gun = NewObject<UPaintGunProfile>();
		UPaintScatterProfile* const Scatter = NewObject<UPaintScatterProfile>();
		Scatter->MuzzleSpeed = MuzzleSpeed;
		UPaintballProfile* const Ball = NewObject<UPaintballProfile>();
		Ball->GravityScale = GravityScale;
		Ball->DropAfter = DropAfter;
		Ball->DropGravityScale = DropGravityScale;
		Gun->Scatter = Scatter;
		Gun->Paintball = Ball;
		return Gun;
	}

	FPaintFireContext MakeContext(UWorld& World, const FVector& ViewDirection)
	{
		FPaintFireContext Context;
		Context.World = &World;
		Context.Muzzle = FTransform(FVector(0.0f, 0.0f, 100.0f));
		Context.ViewOrigin = FVector(0.0f, 0.0f, 150.0f);
		Context.ViewDirection = ViewDirection.GetSafeNormal();
		Context.bAuthority = false;
		return Context;
	}
}

/** 총 프로필의 탄착 예측: 조준점을 맞히는 직사, 조준점 앞에 떨어지는 수평 사격, 2단계 중력, 히트스캔은 예측 없음. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCrosshairPredictImpactTest,
	"MintChoco.Weapons.Crosshair.PredictImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCrosshairPredictImpactTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	// 윗면이 z = 0 인 넓은 바닥.
	MintChocoTest::SpawnBlock(*World, FVector(0.0f, 0.0f, -50.0f), FVector(20000.0f, 20000.0f, 50.0f));

	FVector AimPoint;
	FVector Impact;

	// (a) 바닥을 향해 45도: 조준점도 탄착도 바닥이고 거의 같은 곳.
	{
		const UPaintGunProfile* const Gun = MakeGun(3000.0f, 0.5f, 0.0f, 4.0f);
		const FPaintFireContext Context = MakeContext(*World, FVector(1.0f, 0.0f, -1.0f));
		TestTrue(TEXT("a gun predicts"), Gun->PredictImpact(Context, AimPoint, Impact));
		TestEqual(TEXT("aim point sits on the floor"), static_cast<float>(AimPoint.Z), 0.0f, 1.0f);
		TestEqual(TEXT("aim point 150 cm out"), static_cast<float>(AimPoint.X), 150.0f, 1.0f);
		TestTrue(TEXT("impact lands where the crosshair rests"), FVector::Dist(AimPoint, Impact) < 30.0f);
		TestFalse(TEXT("no shortfall when the ball reaches the aim point"),
			FPaintCrosshairMath::ImpactFallsShort(Context.ViewOrigin, Context.ViewDirection, AimPoint, Impact, 50.0f));
	}

	// (b) 수평으로 허공: 조준점은 트레이스 끝(10000cm), 공은 중력에 바닥으로 떨어진다.
	float HorizontalReach = 0.0f;
	{
		const UPaintGunProfile* const Gun = MakeGun(3000.0f, 0.5f, 0.0f, 4.0f);
		const FPaintFireContext Context = MakeContext(*World, FVector::ForwardVector);
		TestTrue(TEXT("a gun predicts"), Gun->PredictImpact(Context, AimPoint, Impact));
		TestEqual(TEXT("aim trace hits nothing"), static_cast<float>(AimPoint.X), 10000.0f, 1.0f);
		TestTrue(TEXT("the ball comes down on the floor"), Impact.Z < 10.0f);
		TestTrue(TEXT("the ball flies a plausible distance"), Impact.X > 1000.0f && Impact.X < 3000.0f);
		TestTrue(TEXT("a dropped ball falls short of the aim point"),
			FPaintCrosshairMath::ImpactFallsShort(Context.ViewOrigin, Context.ViewDirection, AimPoint, Impact, 50.0f));
		HorizontalReach = static_cast<float>(Impact.X);
	}

	// (c) 직진 0.2초 뒤 급강하: 직진 구간(600cm)은 넘기고, (b)보다 훨씬 앞에 떨어진다.
	{
		const UPaintGunProfile* const Gun = MakeGun(3000.0f, 0.05f, 0.2f, 4.0f);
		const FPaintFireContext Context = MakeContext(*World, FVector::ForwardVector);
		TestTrue(TEXT("a two-phase gun predicts"), Gun->PredictImpact(Context, AimPoint, Impact));
		TestTrue(TEXT("the drop starts after the straight phase"), Impact.X > 600.0f);
		TestTrue(TEXT("the heavy drop lands before the single-gravity ball"), Impact.X < HorizontalReach);
		TestTrue(TEXT("still on the floor"), Impact.Z < 10.0f);
	}

	// (d) 히트스캔은 조준점이 곧 탄착이라 예측하지 않는다.
	{
		const UPaintSniperProfile* const Sniper = NewObject<UPaintSniperProfile>();
		const FPaintFireContext Context = MakeContext(*World, FVector::ForwardVector);
		TestFalse(TEXT("a hitscan profile has no impact prediction"), Sniper->PredictImpact(Context, AimPoint, Impact));
	}
	return true;
}

#endif
