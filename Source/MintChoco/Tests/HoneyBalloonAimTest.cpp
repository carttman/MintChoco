#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/Unit.h"
#include "Items/HoneyBalloonAbility.h"
#include "Items/HoneyBalloonProfile.h"
#include "Items/HoneyBalloonProjectile.h"
#include "Items/ItemAimPreview.h"
#include "Items/ItemSlotComponent.h"
#include "Tests/TestWorld.h"
#include "Weapons/PaintballProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 경고 없이 발동할 수 있는 최소한의 꿀풍선 프로필. 즉발이어야 조준 확정 뒤 바로 끝난다. */
	UHoneyBalloonProfile* MakeHoneyProfile()
	{
		UHoneyBalloonProfile* const Profile = NewObject<UHoneyBalloonProfile>();
		Profile->Duration = 0.0f;
		Profile->AbilityClass = UGA_HoneyBalloon::StaticClass();
		Profile->ProjectileClass = AHoneyBalloonProjectile::StaticClass();
		Profile->Burst.Paintball = NewObject<UPaintballProfile>();
		return Profile;
	}

	int32 CountBalloons(UWorld& World)
	{
		int32 Count = 0;
		for (TActorIterator<AHoneyBalloonProjectile> It(&World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
}

/**
 * 꿀풍선의 조준 흐름: 아이템 키는 던지지 않고, 우클릭은 아이템을 슬롯에 남기며, 좌클릭만이
 * 실제로 던진다.
 *
 * "취소하면 슬롯에 돌아온다"는 되돌리는 코드가 아니라 조준 중에 슬롯을 비우지 않는다는
 * 사실로 지켜진다. 언젠가 발동 시점으로 소모가 되돌아가면 여기가 먼저 깨진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoneyBalloonAimTest,
	"MintChoco.Items.HoneyBalloon.Aim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHoneyBalloonAimTest::RunTest(const FString& Parameters)
{
	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	AUnit* const Unit = World->SpawnActor<AUnit>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator);
	UItemSlotComponent* const Slot = Unit ? Unit->GetItemSlot() : nullptr;
	UAbilitySystemComponent* const AbilitySystem = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
	if (!TestNotNull(TEXT("유닛의 아이템 슬롯"), Slot) || !TestNotNull(TEXT("유닛의 ASC"), AbilitySystem))
	{
		return false;
	}

	// 컨트롤러가 없는 폰이라 빙의 경로가 돌지 않는다. 어빌리티를 굴리려면 이것만 있으면 된다.
	AbilitySystem->InitAbilityActorInfo(Unit, Unit);

	UHoneyBalloonProfile* const Profile = MakeHoneyProfile();
	UAnimSequenceBase* const Pose = NewObject<UAnimSequence>();
	Profile->PoseAnimation = Pose;
	Slot->GiveItem(Profile);
	TestNull(TEXT("들고만 있으면 자세를 잡지 않는다"), Slot->GetItemPose());

	// 아이템 키: 조준만 시작한다.
	TestTrue(TEXT("아이템을 쓰면 어빌리티가 켜진다"), Slot->TryUseHeldItem());
	TestEqual(TEXT("조준 중에는 아이템이 슬롯에 남는다"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Profile));
	TestEqual(TEXT("아이템 키만으로는 아무것도 날아가지 않는다"), CountBalloons(*World), 0);

	// 조준은 태그를 남기지 않으므로, 이 플래그가 다른 머신에 자세를 알리는 유일한 신호다.
	TestTrue(TEXT("슬롯이 조준 중임을 안다"), Slot->IsAimingItem());
	TestEqual(TEXT("조준하는 동안 그 아이템의 자세를 잡는다"), Slot->GetItemPose(), Pose);

	// 우클릭: 물린다. 아이템은 그대로다.
	TestTrue(TEXT("우클릭이 조준을 가져간다"), Slot->HandleCancelInput());
	TestEqual(TEXT("취소해도 아이템은 슬롯에 있다"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Profile));
	TestEqual(TEXT("취소는 아무것도 던지지 않는다"), CountBalloons(*World), 0);
	TestFalse(TEXT("취소하면 조준이 풀린다"), Slot->IsAimingItem());
	TestNull(TEXT("취소하면 자세도 풀린다"), Slot->GetItemPose());

	// 조준이 끝났으므로 입력은 다시 무기 몫이다.
	TestFalse(TEXT("조준이 끝나면 좌클릭은 무기로 간다"), Slot->HandleFireInput());
	TestFalse(TEXT("우클릭도 마찬가지"), Slot->HandleCancelInput());

	// 취소한 아이템은 다시 쓸 수 있어야 한다(스펙에 "끝나면 제거" 표시가 서지 않았으므로).
	TestTrue(TEXT("취소한 아이템을 다시 쓸 수 있다"), Slot->TryUseHeldItem());
	TestEqual(TEXT("다시 조준해도 아이템은 그대로"), Slot->GetHeldItem(), static_cast<UItemProfile*>(Profile));

	// 좌클릭: 이제야 던진다.
	TestTrue(TEXT("좌클릭이 조준을 확정한다"), Slot->HandleFireInput());
	TestNull(TEXT("던지면 슬롯이 빈다"), Slot->GetHeldItem());
	TestEqual(TEXT("좌클릭 한 번에 하나만 날아간다"), CountBalloons(*World), 1);
	TestNull(TEXT("던진 뒤에는 자세가 남지 않는다"), Slot->GetItemPose());

	return true;
}

/**
 * 유지 자세를 고르는 규칙. 애님 블루프린트는 이 값 하나만 보고 자세를 잡으므로, 들고만 있는
 * 아이템의 자세가 새어 나오거나 조준을 끝낸 뒤에도 남으면 캐릭터가 그 포즈로 굳는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemPoseSelectionTest,
	"MintChoco.Items.Animation.PoseSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemPoseSelectionTest::RunTest(const FString& Parameters)
{
	UItemSlotComponent* const Slot = NewObject<UItemSlotComponent>();
	UHoneyBalloonProfile* const Profile = MakeHoneyProfile();
	UAnimSequenceBase* const Pose = NewObject<UAnimSequence>();
	Profile->PoseAnimation = Pose;

	TestNull(TEXT("빈 슬롯은 자세가 없다"), Slot->GetItemPose());

	// 들고 있는 것만으로는 아무 자세도 잡지 않는다. 쓰기 시작해야 한다.
	Slot->GiveItem(Profile);
	TestNull(TEXT("들고만 있으면 자세가 없다"), Slot->GetItemPose());

	Slot->SetAiming(true);
	TestTrue(TEXT("조준 중임을 안다"), Slot->IsAimingItem());
	TestEqual(TEXT("조준 자세는 슬롯의 아이템에서 온다"), Slot->GetItemPose(), Pose);

	// 덮는 범위는 아이템이 정한다. 애님 그래프가 이 값으로 전신 가지와 상체 가지를 가른다.
	TestEqual(TEXT("기본은 전신을 덮는다"), Slot->GetItemPoseBlend(), EItemPoseBlend::FullBody);
	Profile->PoseBlend = EItemPoseBlend::UpperBody;
	TestEqual(TEXT("상체로 정하면 상체만 덮는다"), Slot->GetItemPoseBlend(), EItemPoseBlend::UpperBody);

	Slot->SetAiming(false);
	TestNull(TEXT("조준이 끝나면 자세가 풀린다"), Slot->GetItemPose());

	// 자세가 비어 있는 아이템은 아무것도 덮지 않는다(클립을 아직 안 만든 아이템).
	Profile->PoseAnimation = nullptr;
	Slot->SetAiming(true);
	TestNull(TEXT("자세를 정하지 않은 아이템은 조준해도 자세가 없다"), Slot->GetItemPose());

	return true;
}

/**
 * 미리보기가 실제 투척과 같은 수치를 쓰는지. 표시된 곡선과 터지는 자리가 갈라지면 조준 자체가
 * 거짓말이 되므로, 둘이 같은 함수를 지나는지 여기서 못 박는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoneyBalloonThrowValuesTest,
	"MintChoco.Items.HoneyBalloon.ThrowValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHoneyBalloonThrowValuesTest::RunTest(const FString& Parameters)
{
	UHoneyBalloonProfile* const Profile = MakeHoneyProfile();
	Profile->ThrowSpeed = 1200.0f;

	const FVector Direction = FVector(1.0f, 0.0f, 1.0f).GetSafeNormal();
	TestTrue(TEXT("속도는 방향 × 던지는 속도다"),
		Profile->GetThrowVelocity(Direction).Equals(Direction * 1200.0f));

	// 미리보기가 훑는 굵기는 투사체의 실제 구 반지름에서 온다. 상수로 적어 두면 언젠가 갈라진다.
	const AHoneyBalloonProjectile* const Defaults = AHoneyBalloonProjectile::StaticClass()->GetDefaultObject<AHoneyBalloonProjectile>();
	TestEqual(TEXT("미리보기 반지름은 투사체의 구에서 온다"), Profile->GetProjectileRadius(), Defaults->GetCollisionRadius());
	TestTrue(TEXT("반지름이 0이 아니다"), Profile->GetProjectileRadius() > 0.0f);

	// 투사체 클래스가 없으면 굵기도 없다(선으로 훑는다).
	Profile->ProjectileClass = nullptr;
	TestEqual(TEXT("투사체 클래스가 없으면 0"), Profile->GetProjectileRadius(), 0.0f);

	return true;
}

/**
 * 궤적의 점이 곡선을 따라 고르게 찍히는지. 시뮬레이션 점은 시간 간격이라 빠른 구간이 성글어,
 * 그대로 쓰면 던지는 방향에 따라 점 간격이 들쭉날쭉해진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoneyBalloonArcSpacingTest,
	"MintChoco.Items.HoneyBalloon.ArcSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHoneyBalloonArcSpacingTest::RunTest(const FString& Parameters)
{
	UStaticMesh* const DotMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!TestNotNull(TEXT("점으로 쓸 엔진 구 메시"), DotMesh))
	{
		return false;
	}

	UWorld* const World = MintChocoTest::MakeWorld();
	if (!TestNotNull(TEXT("테스트 월드"), World))
	{
		return false;
	}

	ON_SCOPE_EXIT { MintChocoTest::DestroyWorld(World); };

	FItemAimPreviewStyle Style;
	Style.DotMesh = DotMesh;
	Style.DotSpacing = 50.0f;
	Style.MaxSimTime = 2.0f;

	AItemAimPreview* const Preview = AItemAimPreview::Spawn(*World, nullptr, Style);
	if (!TestNotNull(TEXT("미리보기 액터"), Preview))
	{
		return false;
	}

	// 빈 월드라 막히는 것이 없다. 45도로 던져 곡선이 제대로 휘게 한다.
	const FVector Start(0.0f, 0.0f, 1000.0f);
	const FVector Velocity = FVector(1.0f, 0.0f, 1.0f).GetSafeNormal() * 1200.0f;
	Preview->UpdateArc(Start, Velocity, /*GravityScale=*/1.0f, /*Radius=*/25.0f, /*IgnoreActor=*/nullptr);

	const UInstancedStaticMeshComponent* const Dots = Preview->FindComponentByClass<UInstancedStaticMeshComponent>();
	if (!TestNotNull(TEXT("점 컴포넌트"), Dots))
	{
		return false;
	}

	const int32 Count = Dots->GetInstanceCount();
	if (!TestTrue(TEXT("곡선에 점이 여럿 찍힌다"), Count > 5))
	{
		return false;
	}

	FTransform First;
	Dots->GetInstanceTransform(0, First, /*bWorldSpace=*/true);
	TestTrue(TEXT("첫 점은 던지는 자리에서 시작한다"), First.GetLocation().Equals(Start, 1.0f));

	// 간격은 곡선을 따라 잰 거리다. 직선 거리는 휘는 만큼 조금 짧아지므로 여유를 둔다.
	for (int32 Index = 1; Index < Count; ++Index)
	{
		FTransform Previous;
		FTransform Current;
		Dots->GetInstanceTransform(Index - 1, Previous, true);
		Dots->GetInstanceTransform(Index, Current, true);

		const float Gap = FVector::Dist(Previous.GetLocation(), Current.GetLocation());
		if (!TestTrue(*FString::Printf(TEXT("%d번째 점의 간격 %.1fcm가 50cm 근처다"), Index, Gap),
			Gap > 40.0f && Gap < 55.0f))
		{
			return false;
		}
	}

	// 점 크기는 메시의 원래 크기와 무관하게 DotSize로 맞춰진다.
	const float MeshSize = DotMesh->GetBounds().BoxExtent.GetMax() * 2.0f;
	TestTrue(TEXT("점은 DotSize 크기로 줄어든다"),
		FMath::IsNearlyEqual(First.GetScale3D().X * MeshSize, Style.DotSize, 0.01f));

	return true;
}

#endif
