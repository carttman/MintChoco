#include "Misc/AutomationTest.h"

#include "Audio/SoundBank.h"
#include "Items/HoneyBalloonProfile.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 터질 때 넘기는 파라미터. 에셋에 적힌 값은 그대로 두고 런타임이 정하는 것만 채운다.
 *
 * 전용 사운드 뱅크가 같이 실리는 것이 핵심이다: APaintBurst는 프로필을 모르고 파라미터만
 * 복제받으므로, 여기서 싣지 않으면 꿀풍선의 파열음이 기본 뱅크로 떨어진다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHoneyBalloonBurstParamsTest,
	"MintChoco.Items.HoneyBalloon.BurstParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHoneyBalloonBurstParamsTest::RunTest(const FString& Parameters)
{
	UHoneyBalloonProfile* const Profile = NewObject<UHoneyBalloonProfile>();
	USoundBank* const Bank = NewObject<USoundBank>();
	Profile->Sounds = Bank;
	Profile->Burst.Count = 12;
	Profile->Burst.Speed = 400.0f;
	Profile->Burst.BurstFXScale = 2.5f;

	const FPaintBurstParams Params = Profile->MakeBurstParams(/*PaintId=*/1, /*Seed=*/42);

	TestEqual(TEXT("전용 뱅크가 파라미터에 실린다"), Params.Sounds.Get(), static_cast<const USoundBank*>(Bank));
	TestEqual(TEXT("페인트 id는 런타임 값"), static_cast<int32>(Params.PaintId), 1);
	TestEqual(TEXT("시드는 런타임 값"), Params.Seed, 42);

	TestEqual(TEXT("에셋의 탄 수는 그대로"), Params.Count, 12);
	TestEqual(TEXT("에셋의 속도는 그대로"), Params.Speed, 400.0f);
	TestEqual(TEXT("에셋의 연출 배율은 그대로"), Params.BurstFXScale, 2.5f);

	// 뱅크를 넣지 않은 아이템은 비어 나가고, 재생 쪽이 기본 뱅크로 내려간다.
	UHoneyBalloonProfile* const Plain = NewObject<UHoneyBalloonProfile>();
	TestNull(TEXT("뱅크가 없으면 비어 나간다"), Plain->MakeBurstParams(0, 0).Sounds.Get());

	// Burst 안에도 뱅크 칸이 있지만 여기서 덮인다: 프로필의 것이 유일한 출처다. 에셋에서 그 칸을
	// 채우는 것은 소리가 조용히 사라지는 실수이고, FItemProfileAssetTest가 그것을 잡는다 —
	// 언젠가 이 규칙이 바뀌면 그 검사도 같이 바뀌어야 하므로 여기에 못 박아 둔다.
	UHoneyBalloonProfile* const MisSlotted = NewObject<UHoneyBalloonProfile>();
	MisSlotted->Burst.Sounds = Bank;
	TestNull(TEXT("Burst 안에 넣은 뱅크는 프로필 값으로 덮인다"), MisSlotted->MakeBurstParams(0, 0).Sounds.Get());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
