#include "Misc/AutomationTest.h"

#include "Game/UnitAnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 애니메이션 변수의 순수 계산: 캐릭터 기준 속도 성분, 이동 방향 각. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitAnimMathTest,
	"MintChoco.Anim.UnitVariables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FUnitAnimMathTest::RunTest(const FString& Parameters)
{
	// 캐릭터가 +X를 보고 있다.
	const FRotator Facing(0.0f, 0.0f, 0.0f);
	FVector2D Local = FUnitAnimMath::LocalPlanarSpeed(FVector(300.0f, 0.0f, 50.0f), Facing);
	TestEqual(TEXT("forward speed"), static_cast<float>(Local.X), 300.0f, 1e-3f);
	TestEqual(TEXT("no sideways speed"), static_cast<float>(Local.Y), 0.0f, 1e-3f);
	TestEqual(TEXT("vertical speed is dropped"), FUnitAnimMath::MoveDirectionDegrees(FVector(300.0f, 0.0f, 50.0f), Facing), 0.0f, 1e-3f);

	// 오른쪽(+Y)으로 스트레이프.
	Local = FUnitAnimMath::LocalPlanarSpeed(FVector(0.0f, 200.0f, 0.0f), Facing);
	TestEqual(TEXT("strafe right"), static_cast<float>(Local.Y), 200.0f, 1e-3f);
	TestEqual(TEXT("right is +90"), FUnitAnimMath::MoveDirectionDegrees(FVector(0.0f, 200.0f, 0.0f), Facing), 90.0f, 1e-3f);
	TestEqual(TEXT("left is -90"), FUnitAnimMath::MoveDirectionDegrees(FVector(0.0f, -200.0f, 0.0f), Facing), -90.0f, 1e-3f);
	TestEqual(TEXT("backwards is 180"), FMath::Abs(FUnitAnimMath::MoveDirectionDegrees(FVector(-200.0f, 0.0f, 0.0f), Facing)), 180.0f, 1e-3f);

	// 캐릭터가 +Y를 보고(요 90) +Y로 달리면 앞이다.
	const FRotator FacingY(0.0f, 90.0f, 0.0f);
	Local = FUnitAnimMath::LocalPlanarSpeed(FVector(0.0f, 400.0f, 0.0f), FacingY);
	TestEqual(TEXT("rotated facing: forward"), static_cast<float>(Local.X), 400.0f, 1e-3f);
	TestEqual(TEXT("rotated facing: direction 0"), FUnitAnimMath::MoveDirectionDegrees(FVector(0.0f, 400.0f, 0.0f), FacingY), 0.0f, 1e-3f);
	// 같은 자세로 +X로 가면 오른쪽(-90)이다: 요 90에서 +X는 오른손 쪽이 아니라 왼손 쪽.
	TestEqual(TEXT("rotated facing: +X is left"), FUnitAnimMath::MoveDirectionDegrees(FVector(400.0f, 0.0f, 0.0f), FacingY), -90.0f, 1e-3f);

	// 정지면 0.
	TestEqual(TEXT("still: direction 0"), FUnitAnimMath::MoveDirectionDegrees(FVector::ZeroVector, Facing), 0.0f);

	// 발사 유지: 마지막 발사 후 0.5초 동안만 참.
	TestFalse(TEXT("fire hold: never fired"), FUnitAnimMath::IsFireHoldActive(10.0, -1.0, 0.5f));
	TestTrue(TEXT("fire hold: the shot frame"), FUnitAnimMath::IsFireHoldActive(10.0, 10.0, 0.5f));
	TestTrue(TEXT("fire hold: inside the hold"), FUnitAnimMath::IsFireHoldActive(10.4, 10.0, 0.5f));
	TestTrue(TEXT("fire hold: exactly at the end"), FUnitAnimMath::IsFireHoldActive(10.5, 10.0, 0.5f));
	TestFalse(TEXT("fire hold: after the hold"), FUnitAnimMath::IsFireHoldActive(10.6, 10.0, 0.5f));
	TestFalse(TEXT("fire hold: zero hold never holds"), FUnitAnimMath::IsFireHoldActive(10.0, 10.0, 0.0f));
	TestTrue(TEXT("fire hold: a later shot restarts it"), FUnitAnimMath::IsFireHoldActive(11.2, 10.9, 0.5f));

	return true;
}

#endif
