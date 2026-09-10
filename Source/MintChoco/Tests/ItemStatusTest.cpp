#include "Misc/AutomationTest.h"

#include "GameplayEffect.h"

#include "Game/TeamTypes.h"
#include "Items/ItemAreaEffect.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSettings.h"
#include "Weapons/PaintDeposit.h"

#if WITH_DEV_AUTOMATION_TESTS

/** 광역 효과의 대상 규칙과 상태 GE의 구성. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemStatusRulesTest,
	"MintChoco.Items.Status.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemStatusRulesTest::RunTest(const FString& Parameters)
{
	// 본인 제외, 아군 제외, 상대만.
	TestFalse(TEXT("the user is never affected"), FItemAreaEffect::ShouldAffect(Teams::Mint, Teams::Mint, /*bIsInstigator=*/true));
	TestFalse(TEXT("a teammate is not affected"), FItemAreaEffect::ShouldAffect(Teams::Mint, Teams::Mint, false));
	TestTrue(TEXT("an opponent is affected"), FItemAreaEffect::ShouldAffect(Teams::Choco, Teams::Mint, false));
	TestTrue(TEXT("a teamless victim counts as an opponent"), FItemAreaEffect::ShouldAffect(Teams::None, Teams::Mint, false));

	// 팀이 없는 사용자(샘플 맵)는 본인만 빼고 전부 상대다.
	TestTrue(TEXT("a teamless user affects everyone else"), FItemAreaEffect::ShouldAffect(Teams::Mint, Teams::None, false));
	TestFalse(TEXT("a teamless user still skips themself"), FItemAreaEffect::ShouldAffect(Teams::None, Teams::None, true));

	// 상태 GE: 지속형, SetByCaller 지속시간, 스택 1 갱신. 아이템 GE와 같은 골격이다.
	for (const UClass* const EffectClass : { UGE_Stunned::StaticClass(), UGE_SuperArmor::StaticClass() })
	{
		const UGameplayEffect* const Effect = EffectClass->GetDefaultObject<UGameplayEffect>();
		const FString Name = EffectClass->GetName();
		TestEqual(*FString::Printf(TEXT("%s: has a duration"), *Name), Effect->DurationPolicy, EGameplayEffectDurationType::HasDuration);
		TestEqual(*FString::Printf(TEXT("%s: duration is set by caller"), *Name),
			Effect->DurationMagnitude.GetMagnitudeCalculationType(), EGameplayEffectMagnitudeCalculation::SetByCaller);
		TestEqual(*FString::Printf(TEXT("%s: one stack"), *Name), Effect->StackLimitCount, 1);
		TestEqual(*FString::Printf(TEXT("%s: reapplying refreshes"), *Name),
			Effect->StackDurationRefreshPolicy, EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication);
	}

	// 태그가 실제로 등록되어 있다.
	TestTrue(TEXT("stunned tag"), ItemTags::State_Status_Stunned.GetTag().IsValid());
	TestTrue(TEXT("super armor tag"), ItemTags::State_Status_SuperArmor.GetTag().IsValid());
	TestTrue(TEXT("infinite ammo tag"), ItemTags::State_Item_InfiniteAmmo.GetTag().IsValid());
	TestTrue(TEXT("hero landing tag"), ItemTags::State_Item_HeroLanding.GetTag().IsValid());
	TestTrue(TEXT("status tags are not item tags"),
		!ItemTags::State_Status_Stunned.GetTag().MatchesTag(ItemTags::State_Item));

	// 설정 기본값: 스턴과 슈퍼아머가 0이면 규칙이 조용히 사라진다.
	const UItemSettings& Settings = UItemSettings::Get();
	TestTrue(TEXT("stun duration is positive"), Settings.StunDuration > 0.0f);
	TestTrue(TEXT("super armor duration is positive"), Settings.SuperArmorDuration > 0.0f);
	TestTrue(TEXT("knockback speed is positive"), Settings.KnockbackSpeed > 0.0f);

	// 무기 접촉의 스턴: 기본은 없음(0), 차지 비율에 비례, 뒤따르는 슈퍼아머는 1초.
	FPaintDeposit Deposit;
	TestEqual(TEXT("a deposit stuns nobody by default"), Deposit.StunSecondsFor(1.0f), 0.0f);
	TestEqual(TEXT("weapon super armor defaults to one second"), Deposit.StunSuperArmorDuration, 1.0f);
	Deposit.StunDuration = 1.0f;
	TestEqual(TEXT("full charge: the whole stun"), Deposit.StunSecondsFor(1.0f), 1.0f, 1e-4f);
	TestEqual(TEXT("half charge: half the stun"), Deposit.StunSecondsFor(0.5f), 0.5f, 1e-4f);
	TestEqual(TEXT("charge is clamped"), Deposit.StunSecondsFor(3.0f), 1.0f, 1e-4f);
	TestEqual(TEXT("no charge: no stun"), Deposit.StunSecondsFor(0.0f), 0.0f);

	return true;
}

#endif
