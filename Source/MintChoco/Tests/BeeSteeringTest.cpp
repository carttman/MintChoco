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

	return true;
}

#endif
