#include "Misc/AutomationTest.h"

#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSettings.h"
#include "Audio/SoundBank.h"
#include "Game/GameGameState.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * 뱅크에 반드시 항목이 있어야 하는 태그들. 코드가 부르는데 표에 없으면 그 순간은 그냥
	 * 조용히 지나가므로, 빠진 것을 잡아 주는 곳은 여기뿐이다.
	 */
	TArray<FGameplayTag> RequiredTags()
	{
		return {
			AudioTags::Audio_Weapon_Fire,
			AudioTags::Audio_Weapon_Empty,
			AudioTags::Audio_Weapon_Impact,
			AudioTags::Audio_Unit_Dash,
			AudioTags::Audio_Unit_Land,
			AudioTags::Audio_Unit_Footstep,
			AudioTags::Audio_Unit_Stun_Begin,
			AudioTags::Audio_Unit_SuperArmor_Begin,
			AudioTags::Audio_Item_Announce,
			AudioTags::Audio_Item_Pickup,
			AudioTags::Audio_Item_Activate,
			AudioTags::Audio_World_Burst,
			AudioTags::Audio_World_Splat,
			AudioTags::Audio_World_Balloon_Hit,
			AudioTags::Audio_World_Balloon_Pop,
			AudioTags::Audio_Match_CountdownTick,
			AudioTags::Audio_Match_Start,
			AudioTags::Audio_Match_End_Win,
			AudioTags::Audio_Match_End_Lose,
			AudioTags::Audio_UI_Click,
			AudioTags::Audio_UI_Error,
		};
	}
}

/**
 * 사운드가 "설정은 돼 있는데 아무 소리도 안 나는" 상태로 출하되지 않게 지킨다.
 *
 * 뱅크가 아직 없는 초기 단계에서는 경고만 남기고 통과시킨다. 뱅크가 생긴 뒤로는 코드가
 * 부르는 태그가 표에 다 있는지, 3D 이벤트에 감쇠가 붙어 있는지, 값이 유효한지를 따진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSoundBankTest,
	"MintChoco.Audio.Bank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSoundBankTest::RunTest(const FString& Parameters)
{
	const UGameAudioSettings& Settings = UGameAudioSettings::Get();

	const USoundBank* const Bank = Settings.Bank.LoadSynchronous();
	if (!Bank)
	{
		AddWarning(TEXT("사운드 뱅크가 아직 설정되지 않았습니다(Project Settings > Game Audio). 뱅크가 생기면 이 테스트가 내용을 검사합니다."));
		return true;
	}

	// 아직 아무것도 채우지 않은 뱅크는 "사운드 작업 전"이라는 뜻이지 오류가 아니다.
	// 한 항목이라도 들어오는 순간부터 아래 검사가 전부 켜진다.
	TArray<FGameplayTag> AllTags;
	Bank->GetTags(AllTags);
	if (AllTags.Num() == 0)
	{
		AddWarning(TEXT("사운드 뱅크가 비어 있습니다. 항목을 채우면 이 테스트가 내용을 검사합니다."));
		return true;
	}

	const bool bHasDefaultAttenuation = !Settings.DefaultAttenuation.IsNull();

	// 코드가 부르는 태그가 표에 있고, 실제로 소리가 붙어 있는가.
	for (const FGameplayTag& Tag : RequiredTags())
	{
		const FSoundEvent* const Event = Bank->Find(Tag);
		if (!TestNotNull(*FString::Printf(TEXT("%s: 뱅크에 항목이 있다"), *Tag.ToString()), Event))
		{
			continue;
		}

		TestNotNull(*FString::Printf(TEXT("%s: 소리가 지정돼 있다"), *Tag.ToString()), Event->Sound.Get());
	}

	// 표에 든 모든 항목의 값이 유효한가.
	for (const FGameplayTag& Tag : AllTags)
	{
		const FSoundEvent* const Event = Bank->Find(Tag);
		if (!Event)
		{
			continue;
		}

		TestTrue(*FString::Printf(TEXT("%s: 볼륨이 0보다 크다"), *Tag.ToString()), Event->VolumeMultiplier > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s: 피치 범위가 뒤집히지 않았다"), *Tag.ToString()),
			Event->PitchRange.X > 0.0f && Event->PitchRange.X <= Event->PitchRange.Y);

		// 3D 이벤트에 감쇠가 없으면 거리와 무관하게 같은 크기로 들린다. 이벤트나 설정 어느
		// 쪽이든 하나는 있어야 한다.
		if (!Event->b2D)
		{
			TestTrue(*FString::Printf(TEXT("%s: 3D 이벤트에 감쇠가 있다"), *Tag.ToString()),
				Event->Attenuation != nullptr || bHasDefaultAttenuation);
		}
	}

	// 음악 표가 가리키는 곡도 뱅크에 있어야 한다.
	for (const TPair<EMatchPhase, FGameplayTag>& Pair : Settings.MusicByPhase)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}
		TestNotNull(*FString::Printf(TEXT("음악 %s: 뱅크에 항목이 있다"), *Pair.Value.ToString()), Bank->Find(Pair.Value));
	}

	return true;
}

#endif
