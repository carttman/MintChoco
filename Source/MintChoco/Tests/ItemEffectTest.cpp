#include "Misc/AutomationTest.h"

#include "GameplayEffect.h"

#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/SpeedStarAbility.h"
#include "Items/SweetSpinnerAbility.h"
#include "Items/SpeedStarProfile.h"
#include "Items/SweetSpinnerProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/** GE와 어빌리티의 생성자 구성: 지속형, 스택 1개 갱신, 아이템마다 다른 상태 태그와 GE 클래스. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemEffectConfigTest,
	"MintChoco.Items.Effect.Config",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemEffectConfigTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("state tag registered"), ItemTags::State_Item_SweetSpinner.GetTag().IsValid());
	TestTrue(TEXT("duration tag registered"), ItemTags::Data_Item_Duration.GetTag().IsValid());
	TestTrue(TEXT("leaf tags sit under State.Item"), ItemTags::State_Item_SpeedStar.GetTag().MatchesTag(ItemTags::State_Item));

	for (const UItemGameplayEffect* const Effect : {
		static_cast<const UItemGameplayEffect*>(GetDefault<UGE_SweetSpinner>()),
		static_cast<const UItemGameplayEffect*>(GetDefault<UGE_SpeedStar>())})
	{
		const FString Name = Effect->GetClass()->GetName();
		TestEqual(*FString::Printf(TEXT("%s: has a duration"), *Name), Effect->DurationPolicy, EGameplayEffectDurationType::HasDuration);
		TestEqual(*FString::Printf(TEXT("%s: one stack per target"), *Name), Effect->GetStackingType(), EGameplayEffectStackingType::AggregateByTarget);
		TestEqual(*FString::Printf(TEXT("%s: stack limit 1"), *Name), Effect->StackLimitCount, 1);
		TestEqual(*FString::Printf(TEXT("%s: reuse restarts the timer"), *Name),
			Effect->StackDurationRefreshPolicy, EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication);
		TestEqual(*FString::Printf(TEXT("%s: duration comes from the caller"), *Name),
			Effect->DurationMagnitude.GetMagnitudeCalculationType(), EGameplayEffectMagnitudeCalculation::SetByCaller);
	}

	const UGA_SweetSpinner* const Spinner = GetDefault<UGA_SweetSpinner>();
	const UGA_SpeedStar* const Star = GetDefault<UGA_SpeedStar>();
	TestEqual(TEXT("spinner tag"), Spinner->GetStateTag(), ItemTags::State_Item_SweetSpinner.GetTag());
	TestEqual(TEXT("speed star tag"), Star->GetStateTag(), ItemTags::State_Item_SpeedStar.GetTag());
	TestTrue(TEXT("each item has its own GE class, so stacks never collide"), Spinner->GetEffectClass() != Star->GetEffectClass());

	// 프로필은 어빌리티 클래스에서 상태 태그를 읽는다.
	USweetSpinnerProfile* const SpinnerProfile = NewObject<USweetSpinnerProfile>();
	TestFalse(TEXT("no ability, no tag"), SpinnerProfile->GetStateTag().IsValid());
	SpinnerProfile->AbilityClass = UGA_SweetSpinner::StaticClass();
	TestEqual(TEXT("profile resolves its tag through the ability"), SpinnerProfile->GetStateTag(), ItemTags::State_Item_SweetSpinner.GetTag());
	TestFalse(TEXT("spinner is not a speed item"), SpinnerProfile->GrantsSpeedBoost());
	TestTrue(TEXT("speed star is a speed item"), NewObject<USpeedStarProfile>()->GrantsSpeedBoost());

	return true;
}

#endif
