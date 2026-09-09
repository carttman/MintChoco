#include "Misc/AutomationTest.h"

#include "Game/UnitMovementComponent.h"

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

	return true;
}

#endif
