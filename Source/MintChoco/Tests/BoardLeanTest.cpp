#include "Misc/AutomationTest.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Game/Unit.h"
#include "Game/UnitAnimInstance.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 회전 기울기: 한 프레임의 요 변화를 속도로 바꾸고(경계를 넘어도 짧은 쪽), 도는 자전거처럼
 * atan(속도 × 각속도 / 중력)만큼 회전 안쪽으로 기운다. 빨리 달리며 급하게 돌수록 크게 기운다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardLeanTurnTest,
	"MintChoco.Anim.BoardLean.Turn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardLeanTurnTest::RunTest(const FString& Parameters)
{
	// 회전 속도: 한 프레임의 요 차이 / 시간. 오른쪽(요 증가)이 양수.
	TestEqual(TEXT("turning right is positive"), FUnitAnimMath::YawRateDegrees(10.0, 13.0, 0.01f), 300.0f, 0.01f);
	TestEqual(TEXT("turning left is negative"), FUnitAnimMath::YawRateDegrees(10.0, 7.0, 0.01f), -300.0f, 0.01f);
	TestEqual(TEXT("not turning is zero"), FUnitAnimMath::YawRateDegrees(45.0, 45.0, 0.016f), 0.0f);
	TestEqual(TEXT("no time, no rate"), FUnitAnimMath::YawRateDegrees(0.0, 90.0, 0.0f), 0.0f);

	// 경계: 179에서 -179는 오른쪽으로 2도다(왼쪽으로 358도가 아니다).
	TestEqual(TEXT("wraps across +180"), FUnitAnimMath::YawRateDegrees(179.0, -179.0, 0.01f), 200.0f, 0.01f);
	TestEqual(TEXT("wraps across -180"), FUnitAnimMath::YawRateDegrees(-179.0, 179.0, 0.01f), -200.0f, 0.01f);
	TestEqual(TEXT("wraps across 0/360"), FUnitAnimMath::YawRateDegrees(359.0, 1.0, 0.01f), 200.0f, 0.01f);
	TestEqual(TEXT("accumulated turns fold back"), FUnitAnimMath::YawRateDegrees(725.0, 1.0, 0.01f), -400.0f, 0.01f);

	// 기울기: 대시 속도 1700 cm/s, 중력 자리 10000 cm/s², 최대 25도.
	const float Speed = 1700.0f;
	const float Gravity = 10000.0f;
	const float Max = 25.0f;
	const float Lean90 = FUnitAnimMath::BoardLeanFromTurn(Speed, 90.0f, Gravity, Max);
	TestEqual(TEXT("90 deg/s at dash speed leans about 15 degrees right"), Lean90, 14.951f, 0.05f);
	TestEqual(TEXT("turning left leans left by the same amount"), FUnitAnimMath::BoardLeanFromTurn(Speed, -90.0f, Gravity, Max), -14.951f, 0.05f);
	TestEqual(TEXT("30 deg/s leans about 5 degrees"), FUnitAnimMath::BoardLeanFromTurn(Speed, 30.0f, Gravity, Max), 5.09f, 0.05f);

	TestTrue(TEXT("a sharp turn leans more than a gentle one"),
		FUnitAnimMath::BoardLeanFromTurn(Speed, 90.0f, Gravity, Max) > FUnitAnimMath::BoardLeanFromTurn(Speed, 30.0f, Gravity, Max));
	TestTrue(TEXT("a fast ride leans more than a slow one at the same turn"),
		FUnitAnimMath::BoardLeanFromTurn(Speed, 90.0f, Gravity, Max) > FUnitAnimMath::BoardLeanFromTurn(800.0f, 90.0f, Gravity, Max));

	TestEqual(TEXT("a very sharp turn is clamped to the max"), FUnitAnimMath::BoardLeanFromTurn(Speed, 270.0f, Gravity, Max), 25.0f, 1e-4f);
	TestEqual(TEXT("clamped on the left too"), FUnitAnimMath::BoardLeanFromTurn(Speed, -270.0f, Gravity, Max), -25.0f, 1e-4f);
	TestEqual(TEXT("turning on the spot does not lean"), FUnitAnimMath::BoardLeanFromTurn(0.0f, 180.0f, Gravity, Max), 0.0f);
	TestEqual(TEXT("riding straight does not lean"), FUnitAnimMath::BoardLeanFromTurn(Speed, 0.0f, Gravity, Max), 0.0f);
	TestEqual(TEXT("a negative max flips the direction"), FUnitAnimMath::BoardLeanFromTurn(Speed, 90.0f, Gravity, -Max), -14.951f, 0.05f);
	TestEqual(TEXT("no gravity, no lean"), FUnitAnimMath::BoardLeanFromTurn(Speed, 90.0f, 0.0f, Max), 0.0f);

	return true;
}

/**
 * 메시 기울이기: 캐릭터의 앞 축을 중심으로 굴러 양수면 머리가 오른쪽으로 가고, 0이면 원래 자세로
 * 정확히 돌아온다. 네트워크 스무딩이 매 틱 쓰는 기준 회전(GetBaseRotationOffset)도 함께 바뀌어야
 * 다른 클라이언트에서 기울기가 덮이지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardLeanMeshTest,
	"MintChoco.Anim.BoardLean.Mesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardLeanMeshTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	AUnit* const Unit = World->SpawnActor<AUnit>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator);
	USkeletalMeshComponent* const Mesh = Unit ? Unit->GetMesh() : nullptr;
	if (!TestNotNull(TEXT("유닛의 메시"), Mesh))
	{
		return false;
	}

	// 블루프린트처럼 메시를 요 -90으로 돌려 놓는다. 기울기는 메시 축이 아니라 캐릭터 축으로 가야 한다.
	const FRotator RestRotation(0.0f, -90.0f, 0.0f);
	const FVector RestLocation(0.0f, 0.0f, -90.0f);
	Mesh->SetRelativeLocationAndRotation(RestLocation, RestRotation);
	Unit->CacheInitialMeshOffset(RestLocation, RestRotation);

	const auto UpTowardRight = [Unit, Mesh]()
	{
		return static_cast<float>(FVector::DotProduct(Mesh->GetComponentQuat().GetUpVector(), Unit->GetActorRightVector()));
	};
	const auto UpTowardForward = [Unit, Mesh]()
	{
		return static_cast<float>(FVector::DotProduct(Mesh->GetComponentQuat().GetUpVector(), Unit->GetActorForwardVector()));
	};

	Unit->SetMeshOffset(FRotator(0.0f, 0.0f, 15.0f));
	TestEqual(TEXT("positive lean tips the head right by sin(15)"), UpTowardRight(), FMath::Sin(FMath::DegreesToRadians(15.0f)), 1e-3f);
	TestEqual(TEXT("leaning does not pitch forward"), UpTowardForward(), 0.0f, 1e-3f);
	TestTrue(TEXT("network smoothing target follows the lean"),
		Unit->GetBaseRotationOffset().Equals(Mesh->GetRelativeRotation().Quaternion(), 1e-3f));
	TestTrue(TEXT("leaning keeps the mesh where it was"), Mesh->GetRelativeLocation().Equals(RestLocation, 1e-3f));

	Unit->SetMeshOffset(FRotator(0.0f, 0.0f, -15.0f));
	TestEqual(TEXT("negative lean tips the head left"), UpTowardRight(), -FMath::Sin(FMath::DegreesToRadians(15.0f)), 1e-3f);

	// 여러 번 기울여도 기울기가 쌓이지 않는다: 기준은 처음 자세다.
	Unit->SetMeshOffset(FRotator(0.0f, 0.0f, 10.0f));
	Unit->SetMeshOffset(FRotator(0.0f, 0.0f, 10.0f));
	TestEqual(TEXT("repeated leans do not accumulate"), UpTowardRight(), FMath::Sin(FMath::DegreesToRadians(10.0f)), 1e-3f);

	Unit->SetMeshOffset(FRotator(0.0f, 0.0f, 0.0f));
	TestTrue(TEXT("zero restores the rest rotation"), Mesh->GetRelativeRotation().Quaternion().Equals(RestRotation.Quaternion(), 1e-4f));
	TestTrue(TEXT("zero restores the smoothing target"), Unit->GetBaseRotationOffset().Equals(RestRotation.Quaternion(), 1e-4f));
	TestEqual(TEXT("upright again"), UpTowardRight(), 0.0f, 1e-4f);

	return true;
}

/**
 * 히어로 랜딩 다이브 기울기: 내리꽂는 방향과 몸이 보는 쪽의 차이를 메시가 메운다. 히어로 랜딩
 * 내내 캡슐은 전혀 돌지 않으므로, 이 값이 0이면 몸이 등지는 쪽으로 미끄러지는 그림이 된다.
 *
 * 가장 중요한 것은 수직 낙하 줄이다: 향할 방향이 없을 때 요를 다시 구하면 떨어지는 마지막
 * 순간에 몸이 아무 쪽으로나 홱 돌아버린다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroDiveTiltTest,
	"MintChoco.Anim.HeroDiveTilt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroDiveTiltTest::RunTest(const FString& Parameters)
{
	constexpr double Speed = 3000.0;
	constexpr double MaxPitch = 60.0;

	// 몸이 보는 쪽(+X)으로 45도 내리꽂는다: 돌 요는 없고 그만큼 숙인다.
	const FRotator Forward = FUnitAnimMath::HeroDiveTilt(
		FRotator(-45.0f, 0.0f, 0.0f).Vector() * Speed, /*BodyYaw=*/0.0, /*CurrentTiltYaw=*/0.0f, MaxPitch);
	TestEqual(TEXT("앞으로 꽂으면 돌 요가 없다"), Forward.Yaw, 0.0, 1e-3);
	TestEqual(TEXT("45도 다이브는 45도 숙인다"), Forward.Pitch, -45.0, 1e-3);
	TestEqual(TEXT("다이브는 굴리지 않는다"), Forward.Roll, 0.0, 1e-4);

	// 같은 다이브인데 몸이 오른쪽을 보고 있다. 메시는 그 차이만큼 되돌린다.
	const FRotator Sideways = FUnitAnimMath::HeroDiveTilt(
		FRotator(-45.0f, 0.0f, 0.0f).Vector() * Speed, /*BodyYaw=*/90.0, 0.0f, MaxPitch);
	TestEqual(TEXT("몸이 90도 돌아 있으면 메시가 -90도 돌아 맞춘다"), Sideways.Yaw, -90.0, 1e-3);
	TestEqual(TEXT("도는 것과 숙이는 것은 따로 논다"), Sideways.Pitch, -45.0, 1e-3);

	// 뒤쪽으로 꽂으면 몸을 반바퀴 돌린다.
	const FRotator Backward = FUnitAnimMath::HeroDiveTilt(
		FVector(-1.0f, 0.0f, -1.0f).GetSafeNormal() * Speed, /*BodyYaw=*/0.0, 0.0f, MaxPitch);
	TestEqual(TEXT("뒤로 꽂으면 180도"), FMath::Abs(Backward.Yaw), 180.0, 1e-3);

	// 수직에 가까운 깊은 다이브는 상한에서 멈춘다. 안 그러면 완전히 엎어진 채 땅에 박힌다.
	const FRotator Steep = FUnitAnimMath::HeroDiveTilt(
		FRotator(-80.0f, 0.0f, 0.0f).Vector() * Speed, 0.0, 0.0f, MaxPitch);
	TestEqual(TEXT("깊은 다이브는 상한에서 잘린다"), Steep.Pitch, -MaxPitch, 1e-3);

	// 수직 낙하(높은 곳으로 건너간 뒤의 StartHeroPlunge): 향할 방향이 없으므로 보던 각을 그대로 둔다.
	const FRotator Plunge = FUnitAnimMath::HeroDiveTilt(
		FVector(0.0f, 0.0f, -Speed), /*BodyYaw=*/0.0, /*CurrentTiltYaw=*/37.0f, MaxPitch);
	TestEqual(TEXT("수직 낙하는 보던 요를 그대로 유지한다"), Plunge.Yaw, 37.0, 1e-3);
	TestEqual(TEXT("수직 낙하도 상한까지만 숙인다"), Plunge.Pitch, -MaxPitch, 1e-3);

	// 몸이 어디를 보고 있든 마찬가지다. 방향이 없으면 보던 것을 이어가는 것이 전부다.
	TestEqual(TEXT("수직 낙하는 몸의 요를 다시 재지 않는다"),
		FUnitAnimMath::HeroDiveTilt(FVector(0.0f, 0.0f, -Speed), /*BodyYaw=*/125.0, 37.0f, MaxPitch).Yaw, 37.0, 1e-3);

	// 멈춰 있으면 기울일 일도 없다(호버에서 다이브로 넘어가는 첫 프레임).
	TestTrue(TEXT("속도가 0이면 영 회전"),
		FUnitAnimMath::HeroDiveTilt(FVector::ZeroVector, 0.0, 37.0f, MaxPitch).IsNearlyZero());

	return true;
}

#endif
