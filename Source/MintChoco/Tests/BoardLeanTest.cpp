#include "Misc/AutomationTest.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Game/Unit.h"
#include "Game/UnitAnimInstance.h"
#include "Tests/TestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 보드 기울기 입력: 가속 방향을 몸의 오른쪽 축에 투영한다. 크기는 보지 않으므로 소유자의 입력 가속과
 * 다른 클라이언트의 속도 방향 단위 벡터가 같은 값을 낸다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardLeanInputTest,
	"MintChoco.Anim.BoardLean.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FBoardLeanInputTest::RunTest(const FString& Parameters)
{
	const FRotator Facing(0.0f, 0.0f, 0.0f);

	TestEqual(TEXT("right key leans right"), FUnitAnimMath::BoardLeanInput(FVector(0.0f, 2048.0f, 0.0f), Facing), 1.0f, 1e-4f);
	TestEqual(TEXT("left key leans left"), FUnitAnimMath::BoardLeanInput(FVector(0.0f, -2048.0f, 0.0f), Facing), -1.0f, 1e-4f);
	TestEqual(TEXT("forward does not lean"), FUnitAnimMath::BoardLeanInput(FVector(2048.0f, 0.0f, 0.0f), Facing), 0.0f, 1e-4f);
	TestEqual(TEXT("backward does not lean"), FUnitAnimMath::BoardLeanInput(FVector(-2048.0f, 0.0f, 0.0f), Facing), 0.0f, 1e-4f);
	TestEqual(TEXT("forward + right leans part way"), FUnitAnimMath::BoardLeanInput(FVector(2048.0f, 2048.0f, 0.0f), Facing), UE_INV_SQRT_2, 1e-4f);
	TestEqual(TEXT("no input, no lean"), FUnitAnimMath::BoardLeanInput(FVector::ZeroVector, Facing), 0.0f);
	TestEqual(TEXT("vertical acceleration does not lean"), FUnitAnimMath::BoardLeanInput(FVector(0.0f, 0.0f, 900.0f), Facing), 0.0f);

	// 다른 클라이언트의 폰은 가속이 속도 방향의 단위 벡터다. 크기가 달라도 같은 값이어야 한다.
	TestEqual(TEXT("a simulated proxy's unit vector matches the owner's input"),
		FUnitAnimMath::BoardLeanInput(FVector(0.0f, 1.0f, 0.0f), Facing),
		FUnitAnimMath::BoardLeanInput(FVector(0.0f, 2048.0f, 0.0f), Facing), 1e-4f);

	// 몸이 돌아 있으면 그 기준이다. 요 90에서 앞은 +Y, 오른쪽은 -X.
	const FRotator FacingY(0.0f, 90.0f, 0.0f);
	TestEqual(TEXT("rotated facing: -X is right"), FUnitAnimMath::BoardLeanInput(FVector(-2048.0f, 0.0f, 0.0f), FacingY), 1.0f, 1e-4f);
	TestEqual(TEXT("rotated facing: +Y is forward"), FUnitAnimMath::BoardLeanInput(FVector(0.0f, 2048.0f, 0.0f), FacingY), 0.0f, 1e-4f);

	// 피치와 롤은 무시한다.
	TestEqual(TEXT("pitch is ignored"), FUnitAnimMath::BoardLeanInput(FVector(0.0f, 2048.0f, 0.0f), FRotator(30.0f, 0.0f, 10.0f)), 1.0f, 1e-4f);

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

	Unit->SetMeshLean(15.0f);
	TestEqual(TEXT("positive lean tips the head right by sin(15)"), UpTowardRight(), FMath::Sin(FMath::DegreesToRadians(15.0f)), 1e-3f);
	TestEqual(TEXT("leaning does not pitch forward"), UpTowardForward(), 0.0f, 1e-3f);
	TestTrue(TEXT("network smoothing target follows the lean"),
		Unit->GetBaseRotationOffset().Equals(Mesh->GetRelativeRotation().Quaternion(), 1e-3f));
	TestTrue(TEXT("leaning keeps the mesh where it was"), Mesh->GetRelativeLocation().Equals(RestLocation, 1e-3f));

	Unit->SetMeshLean(-15.0f);
	TestEqual(TEXT("negative lean tips the head left"), UpTowardRight(), -FMath::Sin(FMath::DegreesToRadians(15.0f)), 1e-3f);

	// 여러 번 기울여도 기울기가 쌓이지 않는다: 기준은 처음 자세다.
	Unit->SetMeshLean(10.0f);
	Unit->SetMeshLean(10.0f);
	TestEqual(TEXT("repeated leans do not accumulate"), UpTowardRight(), FMath::Sin(FMath::DegreesToRadians(10.0f)), 1e-3f);

	Unit->SetMeshLean(0.0f);
	TestTrue(TEXT("zero restores the rest rotation"), Mesh->GetRelativeRotation().Quaternion().Equals(RestRotation.Quaternion(), 1e-4f));
	TestTrue(TEXT("zero restores the smoothing target"), Unit->GetBaseRotationOffset().Equals(RestRotation.Quaternion(), 1e-4f));
	TestEqual(TEXT("upright again"), UpTowardRight(), 0.0f, 1e-4f);

	return true;
}

#endif
