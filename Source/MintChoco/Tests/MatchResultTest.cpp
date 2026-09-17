#include "Misc/AutomationTest.h"

#include "Game/MatchResult.h"
#include "Game/MatchResultConfetti.h"
#include "Game/PaintBar.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MatchResultTest
{
	/** 자릿수가 눈에 보이도록 단계 길이를 1초 단위로 잡은 시계. */
	FMatchResultTimeline MakeTimeline()
	{
		FMatchResultTimeline Timeline;
		Timeline.FreezeSeconds = 3.0f;
		Timeline.FadeSeconds = 1.0f;
		Timeline.BarEmptySeconds = 2.0f;
		Timeline.BarTeaserSeconds = 1.0f;
		Timeline.BarRealSeconds = 2.0f;
		Timeline.BarFinishSeconds = 1.0f;
		Timeline.CharacterSeconds = 1.0f;
		Timeline.HoldSeconds = 5.0f;
		return Timeline;
	}

	/** 화면 위 자리. 원근 나누기까지 한 값이라 깊이가 달라져도 같으면 같은 자리에 보인다. */
	FVector2D ScreenOf(const FTransform& View, const FVector& World)
	{
		const FVector Local = View.InverseTransformPosition(World);
		return FVector2D(Local.Y / Local.X, Local.Z / Local.X);
	}

	FMatchResultInput MakeResult(float Left, float Right, int32 Winner)
	{
		FMatchResultInput Result;
		Result.LeftCoverage = Left;
		Result.RightCoverage = Right;
		Result.LeftTeam = Teams::Mint;
		Result.RightTeam = Teams::Choco;
		Result.WinningTeam = Winner;
		Result.TeaserCoverage = 0.1f;
		return Result;
	}
}

/** 결과 연출의 시계: 단계 경계, 전체 길이, 가림막이 "3초 대기" 안에 든다는 것. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultTimelineTest,
	"MintChoco.Match.Result.Timeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultTimelineTest::RunTest(const FString& Parameters)
{
	const FMatchResultTimeline Timeline = MatchResultTest::MakeTimeline();
	const float Tolerance = 1.0e-3f;

	// 가림막은 대기 시간 안에서 내려온다: 경기가 끝나고 정확히 FreezeSeconds 에 화면이 다 덮인다.
	TestEqual(TEXT("the wait shortens by the fade"),
		Timeline.GetPhaseSeconds(EMatchResultPhase::Frozen), 2.0f, Tolerance);
	TestEqual(TEXT("the screen is covered at FreezeSeconds"),
		Timeline.GetPhaseStart(EMatchResultPhase::Reveal), Timeline.FreezeSeconds, Tolerance);

	// 대기가 가림막보다 짧아도 두 단계의 합은 대기 시간 그대로다.
	{
		FMatchResultTimeline Quick = Timeline;
		Quick.FreezeSeconds = 0.4f;
		TestEqual(TEXT("a short wait never runs negative"),
			Quick.GetPhaseSeconds(EMatchResultPhase::Frozen), 0.0f, Tolerance);
		TestEqual(TEXT("a short wait still covers on time"),
			Quick.GetPhaseStart(EMatchResultPhase::Reveal), 0.4f, Tolerance);
	}

	TestEqual(TEXT("the bar starts empty once the screen is open"),
		Timeline.GetPhaseStart(EMatchResultPhase::BarEmpty), 4.0f, Tolerance);
	TestEqual(TEXT("the teaser follows the empty hold"),
		Timeline.GetPhaseStart(EMatchResultPhase::BarTeaser), 6.0f, Tolerance);
	TestEqual(TEXT("the real ratio follows the teaser"),
		Timeline.GetPhaseStart(EMatchResultPhase::BarReal), 7.0f, Tolerance);
	TestEqual(TEXT("the finish follows the real ratio"),
		Timeline.GetPhaseStart(EMatchResultPhase::BarFinish), 9.0f, Tolerance);
	TestEqual(TEXT("the characters move after the finish"),
		Timeline.GetPhaseStart(EMatchResultPhase::Characters), 10.0f, Tolerance);
	TestEqual(TEXT("the shot rests after the characters"),
		Timeline.GetPhaseStart(EMatchResultPhase::Hold), 11.0f, Tolerance);

	// 서버가 로비 복귀를 이만큼 미룬다. 마지막 단계의 끝과 어긋나면 연출 도중에 맵이 넘어간다.
	TestEqual(TEXT("the total ends with the hold"), Timeline.GetTotalSeconds(), 16.0f, Tolerance);
	TestEqual(TEXT("the total is where Idle would start"),
		Timeline.GetPhaseStart(EMatchResultPhase::Idle), Timeline.GetTotalSeconds(), Tolerance);

	// 경과 시간 → 단계.
	TestTrue(TEXT("t=0 is the freeze"), Timeline.GetPhase(0.0f) == EMatchResultPhase::Frozen);
	TestTrue(TEXT("the fade starts at 2s"), Timeline.GetPhase(2.5f) == EMatchResultPhase::FadingOut);
	TestTrue(TEXT("the screen opens at 3s"), Timeline.GetPhase(3.5f) == EMatchResultPhase::Reveal);
	TestTrue(TEXT("the teaser is at 6.5s"), Timeline.GetPhase(6.5f) == EMatchResultPhase::BarTeaser);
	TestTrue(TEXT("the finish is at 9.5s"), Timeline.GetPhase(9.5f) == EMatchResultPhase::BarFinish);
	TestTrue(TEXT("past the end the picture stays"), Timeline.GetPhase(999.0f) == EMatchResultPhase::Hold);

	return true;
}

/** 단계별 바 값과 캐릭터의 행동. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultBarStateTest,
	"MintChoco.Match.Result.BarState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultBarStateTest::RunTest(const FString& Parameters)
{
	const FMatchResultInput Result = MatchResultTest::MakeResult(0.55f, 0.40f, Teams::Mint);
	const float Tolerance = 1.0e-3f;

	// 화면이 열리기 전까지 바는 비어 있다.
	for (const EMatchResultPhase Phase : { EMatchResultPhase::Frozen, EMatchResultPhase::FadingOut,
		EMatchResultPhase::Reveal, EMatchResultPhase::BarEmpty })
	{
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(Phase, Result);
		TestEqual(TEXT("the bar starts empty"), State.LeftCoverage, 0.0f, Tolerance);
		TestEqual(TEXT("the bar starts empty"), State.RightCoverage, 0.0f, Tolerance);
		TestEqual(TEXT("no finish before the real ratio"), State.ForcedKnockoutTeam, Teams::None);
	}

	// 티저는 양쪽 같은 값이고, 격돌 문턱(0.6) 아래라 아직 맞닿지 않는다.
	{
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(EMatchResultPhase::BarTeaser, Result);
		TestEqual(TEXT("the teaser fills both sides alike"), State.LeftCoverage, 0.1f, Tolerance);
		TestEqual(TEXT("the teaser fills both sides alike"), State.RightCoverage, 0.1f, Tolerance);

		const FPaintBarRules Rules;
		const FPaintBarFill Fill = FPaintBarMath::ComputeFill(State.LeftCoverage, State.RightCoverage, Rules.ClashCoverage);
		TestFalse(TEXT("the teaser must not clash early"), Fill.IsClashing());
	}

	// 실제 비율에서는 아직 마무리를 걸지 않는다. 격돌이 보여야 할 구간이다.
	{
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(EMatchResultPhase::BarReal, Result);
		TestEqual(TEXT("the real ratio is the match's"), State.LeftCoverage, 0.55f, Tolerance);
		TestEqual(TEXT("the real ratio is the match's"), State.RightCoverage, 0.40f, Tolerance);
		TestEqual(TEXT("the finish waits one phase"), State.ForcedKnockoutTeam, Teams::None);

		const FPaintBarRules Rules;
		const FPaintBarFill Fill = FPaintBarMath::ComputeFill(State.LeftCoverage, State.RightCoverage, Rules.ClashCoverage);
		TestTrue(TEXT("a full board clashes on its own"), Fill.IsClashing());
	}

	// 마무리부터 끝까지 이긴 팀이 걸린 채로 남는다.
	for (const EMatchResultPhase Phase : { EMatchResultPhase::BarFinish, EMatchResultPhase::Characters, EMatchResultPhase::Hold })
	{
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(Phase, Result);
		TestEqual(TEXT("the winner is pushed from the finish on"), State.ForcedKnockoutTeam, Teams::Mint);
	}

	// 합이 격돌 문턱에 못 미치게 끝난 판은 격돌 없이 마무리로 간다.
	{
		const FMatchResultInput Quiet = MatchResultTest::MakeResult(0.25f, 0.20f, Teams::Mint);
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(EMatchResultPhase::BarReal, Quiet);

		const FPaintBarRules Rules;
		const FPaintBarFill Fill = FPaintBarMath::ComputeFill(State.LeftCoverage, State.RightCoverage, Rules.ClashCoverage);
		TestFalse(TEXT("a half-painted board never clashes"), Fill.IsClashing());
	}

	// 무승부는 마무리를 걸지 않는다.
	{
		const FMatchResultInput Draw = MatchResultTest::MakeResult(0.45f, 0.45f, Teams::None);
		const FMatchResultBarState State = FMatchResultMath::MakeBarState(EMatchResultPhase::Hold, Draw);
		TestEqual(TEXT("a draw has nobody to push"), State.ForcedKnockoutTeam, Teams::None);
	}

	return true;
}

/** 승자는 다가오고 패자는 물러난다. 무승부는 둘 다 그대로. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultStepTest,
	"MintChoco.Match.Result.Step",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultStepTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("the winner approaches"),
		FMatchResultMath::GetStep(Teams::Mint, Teams::Mint) == EMatchResultStep::Approach);
	TestTrue(TEXT("the loser withdraws"),
		FMatchResultMath::GetStep(Teams::Choco, Teams::Mint) == EMatchResultStep::Withdraw);

	TestTrue(TEXT("a draw leaves mint standing"),
		FMatchResultMath::GetStep(Teams::Mint, Teams::None) == EMatchResultStep::Stay);
	TestTrue(TEXT("a draw leaves choco standing"),
		FMatchResultMath::GetStep(Teams::Choco, Teams::None) == EMatchResultStep::Stay);

	// 캐릭터 정의가 없어 비워 둔 자리는 승패와 무관하게 가만히 있는다.
	TestTrue(TEXT("an empty slot stays"),
		FMatchResultMath::GetStep(Teams::None, Teams::Mint) == EMatchResultStep::Stay);

	return true;
}

/**
 * 결과 화면 캐릭터의 자세. 카메라가 어떤 각도든 화면에서는 똑바로 서서 이쪽을 봐야 한다.
 *
 * 메시가 실제로 보는 쪽은 컴포넌트의 +Y(MeshYawOffset -90 이 그렇게 맞춘다), 머리 쪽은 +Z 다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultCharacterPoseTest,
	"MintChoco.Match.Result.CharacterPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultCharacterPoseTest::RunTest(const FString& Parameters)
{
	const float Tolerance = 1.0e-3f;
	const float MeshYaw = -90.0f;

	// 비스듬한 부감과 정수직 탑다운, 둘 다 같은 규칙이어야 한다.
	const TArray<FRotator> Views = { FRotator(-50.0f, -43.43f, 0.0f), FRotator(-90.0f, 0.0f, 0.0f) };
	for (const FRotator& ViewRotation : Views)
	{
		const FTransform View(ViewRotation, FVector(0.0f, 0.0f, 10800.0f));
		const FVector CameraUp = View.GetUnitAxis(EAxis::Z);

		for (const float Side : { -210.0f, 210.0f })
		{
			const FVector Slot = View.TransformPosition(FVector(520.0f, Side, -140.0f));
			const FTransform Pose = FMatchResultMath::MakeCharacterTransform(View, Slot, 0.0f, MeshYaw);
			const FVector Facing = Pose.GetUnitAxis(EAxis::Y);
			const FVector Head = Pose.GetUnitAxis(EAxis::Z);

			// 카메라의 정면 축이 아니라 카메라가 있는 자리를 본다. 화면 가장자리에서 둘은 다르다.
			TestTrue(TEXT("the character looks at the camera itself"),
				Facing.Equals((View.GetLocation() - Pose.GetLocation()).GetSafeNormal(), Tolerance));

			TestTrue(TEXT("the head is square to the face"),
				FMath::IsNearlyZero(FVector::DotProduct(Head, Facing), Tolerance));
			TestTrue(TEXT("the character is the right way up on screen"),
				FVector::DotProduct(Head, CameraUp) > 0.0);

			// 카메라를 보면서도 화면에서 굴러가지는 않는다: 머리는 얼굴과 화면 위쪽이 만드는 면 안에 있다.
			const FVector Sideways = FVector::CrossProduct(Facing, CameraUp).GetSafeNormal();
			TestTrue(TEXT("the character does not roll on screen"),
				FMath::IsNearlyZero(FVector::DotProduct(Head, Sideways), Tolerance));

			TestTrue(TEXT("at rest the character sits exactly on its slot"),
				Pose.GetLocation().Equals(Slot, Tolerance));
		}
	}

	// 화면 가장자리에서는 스크린 정렬(정면 축과 나란)과 확실히 달라야 한다. 같다면 옛 동작으로 돌아간 것이다.
	{
		const FTransform View(FRotator(-90.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 10800.0f));
		const FVector Slot = View.TransformPosition(FVector(520.0f, 210.0f, -140.0f));
		const FTransform Pose = FMatchResultMath::MakeCharacterTransform(View, Slot, 0.0f, MeshYaw);

		TestFalse(TEXT("an off-centre character is not screen-aligned"),
			Pose.GetUnitAxis(EAxis::Y).Equals(-View.GetUnitAxis(EAxis::X), 0.01f));
	}

	// 앞뒤 이동은 화면 위 자리를 건드리면 안 된다. 원근이 있으므로 정면 축과 나란히 움직이면
	// 화면 중심에서 바깥으로 벌어진다 - 시선 광선 위로 움직여야 제자리에서 크기만 달라진다.
	for (const FRotator& ViewRotation : Views)
	{
		const FTransform View(ViewRotation, FVector(0.0f, 0.0f, 10800.0f));
		for (const float Side : { -210.0f, 210.0f })
		{
			const FVector Slot = View.TransformPosition(FVector(520.0f, Side, -140.0f));
			const FVector2D Rest = MatchResultTest::ScreenOf(View, Slot);

			const FTransform Near = FMatchResultMath::MakeCharacterTransform(View, Slot, 90.0f, MeshYaw);
			const FTransform Far = FMatchResultMath::MakeCharacterTransform(View, Slot, -70.0f, MeshYaw);

			TestTrue(TEXT("stepping in keeps the screen position"),
				MatchResultTest::ScreenOf(View, Near.GetLocation()).Equals(Rest, Tolerance));
			TestTrue(TEXT("stepping out keeps the screen position"),
				MatchResultTest::ScreenOf(View, Far.GetLocation()).Equals(Rest, Tolerance));

			// 제자리이되 깊이는 달라져야 한다. 안 그러면 다가오는 것이 보이지 않는다.
			const double RestDepth = View.InverseTransformPosition(Slot).X;
			TestTrue(TEXT("stepping in comes closer"),
				View.InverseTransformPosition(Near.GetLocation()).X < RestDepth - 1.0);
			TestTrue(TEXT("stepping out goes further"),
				View.InverseTransformPosition(Far.GetLocation()).X > RestDepth + 1.0);

			TestTrue(TEXT("stepping does not change the facing"),
				Near.GetRotation().Equals(Far.GetRotation(), Tolerance));
		}
	}

	return true;
}

/**
 * 강제 KO 가 이긴 쪽 게이지를 밀고 진 쪽을 탁하게 하는지. 바 위젯은 진 쪽 상태에 플래그를 세우므로
 * (한쪽의 KO 는 "상대가 이겼다"는 뜻) 과장도 그 반대편으로 들어간다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultForcedKnockoutTest,
	"MintChoco.Match.Result.ForcedKnockout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultForcedKnockoutTest::RunTest(const FString& Parameters)
{
	const FPaintBarRules Rules;
	const float Tolerance = 1.0e-3f;
	const float Exaggeration = 0.08f;

	// 왼쪽(민트)이 이긴 판: 왼쪽 게이지가 더 밀린다.
	const FPaintBarFill Fill = FPaintBarMath::ComputeFill(0.55f, 0.40f, Rules.ClashCoverage);
	const FPaintBarFill Pushed = FPaintBarMath::Exaggerate(Fill, /*IntoLeft=*/0.0f, /*IntoRight=*/Exaggeration);

	TestTrue(TEXT("the winner's gauge grows"), Pushed.Left > Fill.Left);
	TestTrue(TEXT("the loser's gauge shrinks"), Pushed.Right < Fill.Right);
	TestEqual(TEXT("the two still meet exactly"), Pushed.Left + Pushed.Right, 1.0f, Tolerance);

	// 미리보기가 들고 다니는 기본값은 아무도 밀지 않는다.
	const FPaintBarPreview Preview;
	TestEqual(TEXT("a plain preview forces nothing"), Preview.ForcedKnockoutTeam, Teams::None);
	TestFalse(TEXT("Teams::None is not a team"), Teams::IsValidId(Preview.ForcedKnockoutTeam));

	return true;
}

/** 테두리가 나타나는 곡선: 앞이 빠른 페이드와, 1 에서 부풀었다 1 로 정확히 돌아오는 크기. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultPopTest,
	"MintChoco.Match.Result.Pop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultPopTest::RunTest(const FString& Parameters)
{
	const float Tolerance = 1.0e-3f;

	FMatchResultPop Pop;
	Pop.Seconds = 1.0f;
	Pop.PeakScale = 1.2f;
	Pop.PeakAt = 0.25f;

	TestEqual(TEXT("it starts invisible"), Pop.GetOpacity(0.0f), 0.0f, Tolerance);
	TestEqual(TEXT("it ends opaque"), Pop.GetOpacity(Pop.Seconds), 1.0f, Tolerance);

	// 앞이 빠르고 뒤가 느리다. 선형이면 절반 시간에 절반이지만 세제곱은 7/8 이 이미 밝아져 있다.
	TestEqual(TEXT("half the time is seven eighths"), Pop.GetOpacity(0.5f), 0.875f, Tolerance);
	TestTrue(TEXT("a quarter of the time is past half the brightness"), Pop.GetOpacity(0.25f) > 0.5f);

	// 크기는 양 끝에서 정확히 1 이다. 남는 배율이 있으면 화면에 늘어난 그림이 그대로 남는다.
	TestEqual(TEXT("it starts at its own size"), Pop.GetScale(0.0f), 1.0f, Tolerance);
	TestEqual(TEXT("it ends at its own size"), Pop.GetScale(Pop.Seconds), 1.0f, Tolerance);
	TestEqual(TEXT("it swells fullest at PeakAt"), Pop.GetScale(0.25f), Pop.PeakScale, Tolerance);
	TestTrue(TEXT("and it never shrinks below its own size"),
		Pop.GetScale(0.6f) > 1.0f && Pop.GetScale(0.6f) < Pop.PeakScale);

	// 시간 밖은 양 끝으로 잠긴다. 위젯이 한 프레임 더 밀어 넣어도 튀지 않는다.
	TestEqual(TEXT("before the start it is the start"), Pop.GetScale(-1.0f), 1.0f, Tolerance);
	TestEqual(TEXT("after the end it is the end"), Pop.GetOpacity(5.0f), 1.0f, Tolerance);
	TestFalse(TEXT("it is not done halfway"), Pop.IsDone(0.5f));
	TestTrue(TEXT("it is done at the end"), Pop.IsDone(Pop.Seconds));

	// 길이가 0 이면 걸자마자 끝난 상태다.
	FMatchResultPop Instant;
	Instant.Seconds = 0.0f;
	TestEqual(TEXT("a zero-second pop is already opaque"), Instant.GetOpacity(0.0f), 1.0f, Tolerance);
	TestEqual(TEXT("and never swells"), Instant.GetScale(0.0f), 1.0f, Tolerance);
	TestTrue(TEXT("and reports itself done"), Instant.IsDone(0.0f));

	// 배율 1 은 크기를 건드리지 않는다. 밝아지기만 하는 예전 모양이다.
	FMatchResultPop Flat;
	Flat.PeakScale = 1.0f;
	TestEqual(TEXT("a flat pop never moves"), Flat.GetScale(Flat.Seconds * Flat.PeakAt), 1.0f, Tolerance);

	return true;
}

/** 스프라이트 시트에서 칸을 떼어 내는 자. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultSpriteSheetTest,
	"MintChoco.Match.Result.SpriteSheet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultSpriteSheetTest::RunTest(const FString& Parameters)
{
	const float Tolerance = 1.0e-4f;

	FMatchResultSpriteSheet Sheet;
	Sheet.Columns = 5;
	Sheet.Rows = 3;
	Sheet.Inset = 0.0f;

	TestEqual(TEXT("five by three is fifteen cells"), Sheet.GetCellCount(), 15);

	const FBox2f First = Sheet.GetCellUV(0);
	TestEqual(TEXT("the first cell starts at the origin"), First.Min.X, 0.0f, Tolerance);
	TestEqual(TEXT("the first cell starts at the top"), First.Min.Y, 0.0f, Tolerance);
	TestEqual(TEXT("and is one fifth wide"), First.Max.X, 0.2f, Tolerance);
	TestEqual(TEXT("and one third tall"), First.Max.Y, 1.0f / 3.0f, Tolerance);

	// 칸은 왼쪽 위에서 가로로 세어 나간다: 5 번은 둘째 줄의 첫 칸이다.
	const FBox2f SecondRow = Sheet.GetCellUV(5);
	TestEqual(TEXT("the sixth cell starts the second row"), SecondRow.Min.X, 0.0f, Tolerance);
	TestEqual(TEXT("one row down"), SecondRow.Min.Y, 1.0f / 3.0f, Tolerance);

	const FBox2f Last = Sheet.GetCellUV(14);
	TestEqual(TEXT("the last cell ends at the right edge"), Last.Max.X, 1.0f, Tolerance);
	TestEqual(TEXT("and at the bottom edge"), Last.Max.Y, 1.0f, Tolerance);

	// 칸 수를 넘는 번호는 감는다. 시트를 줄여도 뽑는 쪽이 터지지 않는다.
	TestEqual(TEXT("a number past the end wraps"), Sheet.GetCellUV(15).Min.X, First.Min.X, Tolerance);
	TestEqual(TEXT("and wraps to the same row"), Sheet.GetCellUV(15).Min.Y, First.Min.Y, Tolerance);

	// 여백은 칸을 사방에서 안쪽으로 물린다. 이웃 칸이 새어 들어오지 않게 하는 값이다.
	Sheet.Inset = 0.01f;
	const FBox2f Trimmed = Sheet.GetCellUV(0);
	TestTrue(TEXT("the inset pulls the left edge in"), Trimmed.Min.X > First.Min.X);
	TestTrue(TEXT("and the right edge in"), Trimmed.Max.X < First.Max.X);

	return true;
}

/** 쏟아지는 스티커: 뿌리는 박자, 같은 씨앗의 같은 그림, 언젠가 다 사라지는 것. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchResultConfettiTest,
	"MintChoco.Match.Result.Confetti",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMatchResultConfettiTest::RunTest(const FString& Parameters)
{
	const float Tolerance = 1.0e-4f;
	const int32 CellCount = 15;

	// 폭을 0 으로 좁혀 흔들림이 아니라 박자만 본다.
	FMatchResultConfettiRules Rules;
	Rules.Count = 20;
	Rules.SpawnSeconds = 1.0f;
	Rules.Life = FFloatInterval(2.0f, 2.0f);
	Rules.FallSpeed = FFloatInterval(0.5f, 0.5f);
	Rules.Scale = FFloatInterval(1.0f, 1.0f);

	FMatchResultConfetti Confetti;
	Confetti.Start(Rules, CellCount, 1234);
	TestEqual(TEXT("nothing is out before the first frame"), Confetti.GetStickers().Num(), 0);
	TestFalse(TEXT("but there is still work to do"), Confetti.IsDone());

	// 뿌리는 시간이 끝나면 정확히 Count 장이 나가 있다. 아직 아무도 죽지 않았다.
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Confetti.Advance(0.1f);
	}
	TestEqual(TEXT("every sticker is out by SpawnSeconds"), Confetti.GetStickers().Num(), Rules.Count);

	for (const FMatchResultSticker& Sticker : Confetti.GetStickers())
	{
		TestTrue(TEXT("the cell is on the sheet"), Sticker.Cell >= 0 && Sticker.Cell < CellCount);
		TestTrue(TEXT("and it is on its way down"), Sticker.Velocity.Y > 0.0f);
	}

	// 씨앗이 같으면 같은 그림이다. 머신마다 따로 뿌려도 같은 화면이 나온다는 뜻이다.
	FMatchResultConfetti Twin;
	Twin.Start(Rules, CellCount, 1234);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Twin.Advance(0.1f);
	}
	TestEqual(TEXT("the same seed spawns as many"), Twin.GetStickers().Num(), Confetti.GetStickers().Num());
	TestEqual(TEXT("in the same place"), Twin.GetStickers()[0].Position.X, Confetti.GetStickers()[0].Position.X, Tolerance);
	TestEqual(TEXT("from the same cell"), Twin.GetStickers()[0].Cell, Confetti.GetStickers()[0].Cell);

	// 수명이 다하면 사라진다. 연출이 길어져도 빈 배열만 돌 뿐이다.
	for (int32 Step = 0; Step < 100; ++Step)
	{
		Confetti.Advance(0.1f);
	}
	TestTrue(TEXT("everything is gone in the end"), Confetti.GetStickers().IsEmpty());
	TestTrue(TEXT("and the field knows it is finished"), Confetti.IsDone());

	// 투명도는 태어날 때 밝아지고 죽기 전에 사라진다.
	FMatchResultSticker Sticker;
	Sticker.Life = 2.0f;
	Sticker.Age = 0.0f;
	TestEqual(TEXT("a newborn is invisible"), Rules.GetOpacity(Sticker), 0.0f, Tolerance);
	Sticker.Age = Rules.FadeInSeconds;
	TestEqual(TEXT("and fully there once it has faded in"), Rules.GetOpacity(Sticker), 1.0f, Tolerance);
	Sticker.Age = Sticker.Life;
	TestEqual(TEXT("and gone at the end of its life"), Rules.GetOpacity(Sticker), 0.0f, Tolerance);

	// 뿌리는 시간이 0 이면 첫 프레임에 다 나가 한 번에 터진다.
	FMatchResultConfettiRules Burst = Rules;
	Burst.SpawnSeconds = 0.0f;

	FMatchResultConfetti AtOnce;
	AtOnce.Start(Burst, CellCount, 1);
	AtOnce.Advance(0.016f);
	TestEqual(TEXT("zero spawn time is one burst"), AtOnce.GetStickers().Num(), Burst.Count);

	return true;
}

#endif
