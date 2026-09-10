#include "Misc/AutomationTest.h"

#include "Items/BeeProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 꿀벌 조종: 막힘이 없으면 원하는 방향, 막히면 순서대로 비켜 가고, 회전은 속도 제한을 지킨다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBeeSteeringTest,
	"MintChoco.Items.Bee.Steering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBeeSteeringTest::RunTest(const FString& Parameters)
{
	const FVector Desired(1.0f, 0.0f, 0.0f);
	TArray<FVector> Candidates;
	FBeeSteering::BuildCandidates(Desired, Candidates);
	TestEqual(TEXT("nine candidates"), Candidates.Num(), 9);
	TestTrue(TEXT("first candidate is the desired direction"), Candidates[0].Equals(Desired));

	bool bAllUnit = true;
	for (const FVector& Candidate : Candidates)
	{
		bAllUnit &= Candidate.IsNormalized();
	}
	TestTrue(TEXT("candidates are unit vectors"), bAllUnit);

	// 좌우 30도는 요만 돌린 것이다.
	TestEqual(TEXT("second candidate is 30 degrees off"), static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Candidates[0], Candidates[1])))), 30.0f, 0.01f);
	TestTrue(TEXT("yaw candidates stay level"), FMath::IsNearlyZero(Candidates[1].Z) && FMath::IsNearlyZero(Candidates[6].Z));
	TestEqual(TEXT("eighth candidate climbs 45 degrees"), static_cast<float>(FMath::RadiansToDegrees(FMath::Asin(Candidates[7].Z))), 45.0f, 0.01f);
	TestTrue(TEXT("last candidate is straight up"), Candidates.Last().Equals(FVector::UpVector));

	// 선택.
	TArray<bool> Blocked;
	TestTrue(TEXT("nothing blocked: desired"), FBeeSteering::Choose(Candidates, Blocked).Equals(Candidates[0]));
	Blocked = { true };
	TestTrue(TEXT("front blocked: first side candidate"), FBeeSteering::Choose(Candidates, Blocked).Equals(Candidates[1]));
	Blocked = { true, true, true, false };
	TestTrue(TEXT("skips every blocked candidate"), FBeeSteering::Choose(Candidates, Blocked).Equals(Candidates[3]));
	Blocked.Init(true, Candidates.Num());
	TestTrue(TEXT("everything blocked: up"), FBeeSteering::Choose(Candidates, Blocked).Equals(FVector::UpVector));

	// 회전 제한.
	const FVector Turned = FBeeSteering::TurnTowards(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), 10.0f);
	TestEqual(TEXT("turns at most the allowed angle"), static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Turned, FVector(1.0f, 0.0f, 0.0f))))), 10.0f, 0.05f);
	TestTrue(TEXT("a small turn lands on the target"), FBeeSteering::TurnTowards(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), 180.0f).Equals(FVector(0.0f, 1.0f, 0.0f), 1e-3f));
	TestTrue(TEXT("no current direction: target"), FBeeSteering::TurnTowards(FVector::ZeroVector, FVector(0.0f, 0.0f, 1.0f), 5.0f).Equals(FVector::UpVector));

	// 고도: 오차에 비례하고 최대 기울기에서 잘린다. 문턱값이 아니라 0 근처에서 부호가 튀지 않는다.
	TestEqual(TEXT("on altitude: level"), FBeeSteering::VerticalComponent(0.0f, 150.0f), 0.0f);
	TestEqual(TEXT("slightly low: gentle climb"), FBeeSteering::VerticalComponent(15.0f, 150.0f), 0.1f, 1e-4f);
	TestEqual(TEXT("slightly high: gentle dive"), FBeeSteering::VerticalComponent(-15.0f, 150.0f), -0.1f, 1e-4f);
	TestEqual(TEXT("far below: capped climb"), FBeeSteering::VerticalComponent(1000.0f, 150.0f), 0.7f);
	TestEqual(TEXT("far above: capped dive"), FBeeSteering::VerticalComponent(-1000.0f, 150.0f), -0.7f);

	// 수평 방향과 Z 성분을 합치면 단위 벡터이고 수평 방향은 유지된다.
	const FVector Combined = FBeeSteering::Combine(FVector(1.0f, 0.0f, 0.0f), 0.6f);
	TestTrue(TEXT("combined is unit length"), Combined.IsNormalized());
	TestEqual(TEXT("combined keeps the vertical"), static_cast<float>(Combined.Z), 0.6f, 1e-4f);
	TestTrue(TEXT("combined keeps the heading"), Combined.X > 0.7f && FMath::IsNearlyZero(Combined.Y));
	TestTrue(TEXT("no heading: straight up or down"), FBeeSteering::Combine(FVector::ZeroVector, -0.3f).Equals(FVector::DownVector));

	// 요와 피치를 따로 돌린다: 뒤로 크게 꺾이며 내려갈 때 호가 수직 아래를 지나지 않는다.
	const FVector Split = FBeeSteering::TurnTowardsSplit(FVector(1.0f, 0.0f, 0.0f), FVector(-0.71f, 0.0f, 0.0f), -0.7f, 6.0f);
	TestTrue(TEXT("split turn stays nearly level"), Split.Z > -0.15f && Split.Z < 0.0f);
	TestEqual(TEXT("split turn rotates the heading by the limit"),
		static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FVector(Split.X, Split.Y, 0.0f).GetSafeNormal(), FVector(1.0f, 0.0f, 0.0f))))), 6.0f, 0.1f);
	TestTrue(TEXT("split turn is unit length"), Split.IsNormalized());
	const FVector Settled = FBeeSteering::TurnTowardsSplit(FVector(-0.71f, 0.0f, -0.7f), FVector(-1.0f, 0.0f, 0.0f), -0.7f, 180.0f);
	TestTrue(TEXT("a free turn lands on the wanted heading and vertical"), Settled.Equals(FVector(-0.714f, 0.0f, -0.7f), 1e-2f));

	// 지면 여유에 비례한 하강 제한: 바닥 바로 위에서는 내려가지 못한다.
	TestEqual(TEXT("no clearance: no descent"), FBeeSteering::MaxDescent(0.0f, 150.0f), 0.0f);
	TestEqual(TEXT("half clearance: half dive"), FBeeSteering::MaxDescent(75.0f, 150.0f), 0.35f, 1e-4f);
	TestEqual(TEXT("full clearance: full dive"), FBeeSteering::MaxDescent(600.0f, 150.0f), 0.7f);

	return true;
}

#endif
