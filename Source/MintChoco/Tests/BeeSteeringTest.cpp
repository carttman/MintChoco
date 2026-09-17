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

	// 베지어 접선: t=0은 P0→P1, t=1은 P1→P2.
	const FVector P0(0.0f, 0.0f, 0.0f);
	const FVector P1(100.0f, 0.0f, 0.0f);
	const FVector P2(100.0f, 100.0f, 0.0f);
	TestTrue(TEXT("bezier tangent at start follows P0->P1"), FBeeSteering::BezierTangent(P0, P1, P2, 0.0f).Equals(FVector(200.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("bezier tangent at end follows P1->P2"), FBeeSteering::BezierTangent(P0, P1, P2, 1.0f).Equals(FVector(0.0f, 200.0f, 0.0f)));

	// 곡선 추적: 이미 목표를 보고 있으면 직진과 같고, 옆의 목표에는 직진보다 덜 꺾은 방향이 나온다.
	const FVector Here(0.0f, 0.0f, 100.0f);
	const FVector Ahead(1.0f, 0.0f, 0.0f);
	TestTrue(TEXT("curve toward a target straight ahead is straight"),
		FBeeSteering::CurveHeading(Here, Ahead, FVector(1000.0f, 0.0f, 50.0f), 0.5f, 0.2f).Equals(Ahead, 1e-3f));
	const FVector Curved = FBeeSteering::CurveHeading(Here, Ahead, FVector(0.0f, 1000.0f, 50.0f), 0.5f, 0.2f);
	TestTrue(TEXT("curve heading is a flat unit vector"), Curved.IsNormalized() && FMath::IsNearlyZero(Curved.Z));
	TestTrue(TEXT("curve keeps some of the current heading"), Curved.X > 0.3f);
	TestTrue(TEXT("curve bends toward the target"), Curved.Y > 0.3f);
	TestTrue(TEXT("zero tension aims straight at the target"),
		FBeeSteering::CurveHeading(Here, Ahead, FVector(0.0f, 1000.0f, 50.0f), 0.0f, 0.2f).Equals(FVector(0.0f, 1.0f, 0.0f), 1e-3f));
	TestTrue(TEXT("a larger lookahead bends more"),
		FBeeSteering::CurveHeading(Here, Ahead, FVector(0.0f, 1000.0f, 50.0f), 0.5f, 0.8f).Y > Curved.Y);
	TestTrue(TEXT("on top of the target: keep heading"), FBeeSteering::CurveHeading(Here, Ahead, Here, 0.5f, 0.2f).Equals(Ahead));

	// 요동: 시작은 0, 1/4 주기에서 최대, 목표 근처에서는 잦아든다.
	TestEqual(TEXT("wobble starts at zero"), FBeeSteering::WobbleYawDeg(0.0f, 25.0f, 1.0f, 1000.0f, 300.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("wobble peaks at a quarter period"), FBeeSteering::WobbleYawDeg(0.25f, 25.0f, 1.0f, 1000.0f, 300.0f), 25.0f, 1e-3f);
	TestEqual(TEXT("wobble halves at half the settle distance"), FBeeSteering::WobbleYawDeg(0.25f, 25.0f, 1.0f, 150.0f, 300.0f), 12.5f, 1e-3f);
	TestEqual(TEXT("wobble is gone on the target"), FBeeSteering::WobbleYawDeg(0.25f, 25.0f, 1.0f, 0.0f, 300.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("no amplitude: no wobble"), FBeeSteering::WobbleYawDeg(0.25f, 0.0f, 1.0f, 1000.0f, 300.0f), 0.0f);

	// 상하 요동: 같은 규칙에 주파수만 다르다. 좌우와 정수비가 아니면 두 파의 비율이 시각마다 달라
	// 합성 방향이 한 줄에 머물지 않는다.
	TestEqual(TEXT("pitch wobble starts at zero"), FBeeSteering::WobblePitchDeg(0.0f, 12.0f, 2.3f, 1000.0f, 300.0f), 0.0f, 1e-4f);
	TestEqual(TEXT("pitch wobble peaks at its own quarter period"), FBeeSteering::WobblePitchDeg(0.25f / 2.3f, 12.0f, 2.3f, 1000.0f, 300.0f), 12.0f, 1e-3f);
	TestEqual(TEXT("pitch wobble is gone on the target"), FBeeSteering::WobblePitchDeg(0.25f / 2.3f, 12.0f, 2.3f, 0.0f, 300.0f), 0.0f, 1e-4f);
	const float RatioA = FBeeSteering::WobblePitchDeg(0.1f, 12.0f, 2.3f, 1000.0f, 300.0f) / FBeeSteering::WobbleYawDeg(0.1f, 25.0f, 1.5f, 1000.0f, 300.0f);
	const float RatioB = FBeeSteering::WobblePitchDeg(0.2f, 12.0f, 2.3f, 1000.0f, 300.0f) / FBeeSteering::WobbleYawDeg(0.2f, 25.0f, 1.5f, 1000.0f, 300.0f);
	TestTrue(TEXT("different frequencies: the wobble direction changes over time"), !FMath::IsNearlyEqual(RatioA, RatioB, 1e-2f));
	TestEqual(TEXT("pitch to vertical: 30 degrees is half"), FBeeSteering::PitchToVertical(30.0f), 0.5f, 1e-4f);
	TestEqual(TEXT("pitch to vertical: zero is level"), FBeeSteering::PitchToVertical(0.0f), 0.0f, 1e-4f);

	return true;
}

/**
 * 꿀벌의 프로필이 클라이언트까지 간다는 것. 조종과 효과는 서버만 하지만 클라이언트의 복사본도
 * 제 비행음을 내야 하고, 그 뱅크는 프로필에만 있다.
 *
 * 복제를 떼도 리슨 호스트에서는 멀쩡히 들리기 때문에 손으로는 알아채기 어렵다. 소리가 클라이언트
 * 한쪽에서만 사라지는 재현 어려운 버그라 여기서 못 박는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBeeProfileReplicationTest,
	"MintChoco.Items.Bee.ProfileReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBeeProfileReplicationTest::RunTest(const FString& Parameters)
{
	const FProperty* const Profile = ABeeProjectile::StaticClass()->FindPropertyByName(TEXT("Profile"));
	if (!TestNotNull(TEXT("꿀벌에 Profile 프로퍼티가 있다"), Profile))
	{
		return false;
	}

	TestTrue(TEXT("Profile이 복제로 표시돼 있다"), Profile->HasAnyPropertyFlags(CPF_Net));

	// GetLifetimeReplicatedProps에 등록됐는지는 여기서 보지 않는다. 표시만 하고 등록을 빠뜨리면
	// 엔진이 그 자리에서 verify로 죽으므로(RegisterReplicatedLifetimeProperty) 조용히 지나갈 수
	// 없고, 확인하겠다고 여기서 그 함수를 부르면 실패가 아니라 런 전체가 죽는다. 조용한 쪽은
	// 위의 표시가 사라지는 경우뿐이라 그것만 지킨다.

	return true;
}

#endif
