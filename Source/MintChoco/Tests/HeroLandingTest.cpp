#include "Misc/AutomationTest.h"

#include "Game/UnitMovementComponent.h"
#include "Items/HeroLandingProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 히어로 랜딩의 의도 플래그와 단계 상태가 무브먼트 컴포넌트에서 일관되게 다뤄진다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingMovementTest,
	"MintChoco.Items.HeroLanding.Movement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingMovementTest::RunTest(const FString& Parameters)
{
	UUnitMovementComponent* const Movement = NewObject<UUnitMovementComponent>();
	Movement->MovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 600.0f;

	// 기본값: 점프대 높이.
	const FHeroLandingParams& Defaults = Movement->GetHeroLandingParams();
	TestEqual(TEXT("default rise height matches the jump pad apex"), Defaults.RiseHeight, 735.0f);
	TestTrue(TEXT("default hover is two seconds"), FMath::IsNearlyEqual(Defaults.HoverTime, 2.0f));

	FHeroLandingParams Params;
	Params.RiseHeight = 500.0f;
	Params.MaxAimDistance = 1000.0f;
	Movement->SetHeroLandingParams(Params);
	TestEqual(TEXT("params are stored"), Movement->GetHeroLandingParams().MaxAimDistance, 1000.0f);

	// 의도 플래그.
	TestFalse(TEXT("starts without intent"), Movement->WantsHeroLanding());
	TestEqual(TEXT("starts in no phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	Movement->SetWantsHeroLanding(true);
	TestTrue(TEXT("intent is remembered"), Movement->WantsHeroLanding());
	TestEqual(TEXT("intent alone does not start a phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestEqual(TEXT("speed unaffected before the phase starts"), Movement->GetMaxSpeed(), 600.0f);

	// 서버 경로: 소유 유닛이 없으면 인정 검사는 통과한다.
	Movement->SetWantsHeroLanding(false);
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_2);
	TestTrue(TEXT("FLAG_Custom_2 restores the intent"), Movement->WantsHeroLanding());
	TestFalse(TEXT("FLAG_Custom_2 alone leaves boost off"), Movement->WantsSpeedBoost());
	Movement->UpdateFromCompressedFlags(0);
	TestFalse(TEXT("a move without the flag clears the intent"), Movement->WantsHeroLanding());

	// 착지 알림은 내리꽂기 중이 아니면 거짓이다.
	TestFalse(TEXT("a plain landing is not a hero landing"), Movement->FinishHeroLandingDive());

	// 중단은 플래그도 내린다.
	Movement->SetWantsHeroLanding(true);
	Movement->AbortHeroLanding();
	TestFalse(TEXT("abort clears the intent"), Movement->WantsHeroLanding());
	TestEqual(TEXT("abort leaves no phase"), Movement->GetHeroLandingPhase(), EHeroLandingPhase::None);
	TestEqual(TEXT("abort outside the custom mode keeps the mode"), Movement->MovementMode, static_cast<TEnumAsByte<EMovementMode>>(MOVE_Walking));

	// 조기 낙하 의도는 별도의 플래그를 탄다. 랜딩 플래그와 서로를 건드리지 않아야,
	// 좌클릭이 랜딩 자체를 끝내 버리는 일이 생기지 않는다.
	Movement->UpdateFromCompressedFlags(0);
	TestFalse(TEXT("starts without a dive intent"), Movement->WantsHeroDive());
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_3);
	TestTrue(TEXT("FLAG_Custom_3 restores the dive intent"), Movement->WantsHeroDive());
	TestFalse(TEXT("the dive flag alone does not arm the landing"), Movement->WantsHeroLanding());
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_2);
	TestFalse(TEXT("a move without the dive flag clears it"), Movement->WantsHeroDive());
	TestTrue(TEXT("the landing intent survives on its own flag"), Movement->WantsHeroLanding());

	// 단계가 없으면 충전량은 0이다. 호버 시간으로만 자라므로 시작 전에 쌓일 곳이 없다.
	TestEqual(TEXT("charge is zero before hovering"), Movement->GetHeroCharge(), 0.0f);

	// 중단은 조기 낙하 의도도 함께 내린다. 남겨 두면 다음 발동의 호버가 첫 프레임에 끝난다.
	Movement->SetWantsHeroDive(true);
	Movement->AbortHeroLanding();
	TestFalse(TEXT("abort clears the dive intent too"), Movement->WantsHeroDive());

	return true;
}

/** 충전량이 착지 효과 배율로 바뀌는 식. 미리보기 원과 실제 반경이 같은 식을 쓴다. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHeroLandingChargeTest,
	TEXT("MintChoco.Items.HeroLanding.Charge"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHeroLandingChargeTest::RunTest(const FString& Parameters)
{
	UHeroLandingProfile* const Profile = NewObject<UHeroLandingProfile>();

	// 끝까지 버티면 프로필에 적힌 값 그대로다. 충전이 최대치를 넘겨 키우는 일은 없다.
	TestEqual(TEXT("a full hover keeps the profile values"), Profile->ChargeScaleFor(1.0f), 1.0f);
	TestEqual(TEXT("an instant release gives the floor"), Profile->ChargeScaleFor(0.0f), Profile->MinChargeScale);

	// 사이는 선형이다. 절반 버티면 바닥과 최대의 가운데.
	TestTrue(TEXT("half a hover lands halfway"),
		FMath::IsNearlyEqual(Profile->ChargeScaleFor(0.5f), (Profile->MinChargeScale + 1.0f) * 0.5f));

	// 범위 밖 입력은 잘린다. 충전량은 무브먼트가 시간으로 세는 값이라 이론상 넘칠 수 없지만,
	// 여기서 잘라 두면 데이터가 이상해도 효과가 폭주하지 않는다.
	TestEqual(TEXT("charge above one is clamped"), Profile->ChargeScaleFor(3.0f), 1.0f);
	TestEqual(TEXT("negative charge is clamped"), Profile->ChargeScaleFor(-1.0f), Profile->MinChargeScale);

	return true;
}

#endif
