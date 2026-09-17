#include "Misc/AutomationTest.h"

#include "Game/PaintGaugeWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 잉크 게이지가 값을 따라가는 방식. 한 발 쏠 때마다 계단처럼 뚝 떨어지지 않게 지수 보간으로
 * 좇되, 목표에 닿기는 해야 한다 — 지수 보간은 원래 목표에 닿지 않아서, 붙이지 않으면 가득 찬
 * 잉크가 영원히 조금 모자라 보인다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintGaugeMathTest,
	"MintChoco.Game.PaintGauge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintGaugeMathTest::RunTest(const FString& Parameters)
{
	// 잉크는 이미 0~1이지만 복제 값이라 범위를 믿지 않는다.
	TestEqual(TEXT("가득"), FPaintGaugeMath::FillFromInk(1.0f), 1.0f);
	TestEqual(TEXT("비었다"), FPaintGaugeMath::FillFromInk(0.0f), 0.0f);
	TestEqual(TEXT("1을 넘으면 가득에서 멈춘다"), FPaintGaugeMath::FillFromInk(1.7f), 1.0f);
	TestEqual(TEXT("음수는 빈 것으로 본다"), FPaintGaugeMath::FillFromInk(-0.3f), 0.0f);

	// 보간 시간이 없으면 곧바로 목표다. 시간이 흐르지 않은 프레임도 마찬가지로 다루지 않는다.
	TestEqual(TEXT("보간 시간이 0이면 바로 목표"), FPaintGaugeMath::Approach(0.0f, 1.0f, 0.016f, 0.0f), 1.0f);
	TestEqual(TEXT("멈춘 프레임도 바로 목표"), FPaintGaugeMath::Approach(0.0f, 1.0f, 0.0f, 0.2f), 1.0f);

	// 한 걸음은 목표 쪽으로 가되 지나치지 않는다.
	const float Step = FPaintGaugeMath::Approach(0.0f, 1.0f, 0.016f, 0.2f);
	TestTrue(TEXT("한 걸음은 목표 쪽으로 간다"), Step > 0.0f && Step < 1.0f);

	// 한 시간상수(0.2초)면 차이의 약 63%를 좁힌다. 이 값이 "따라가는 시간"의 뜻이다.
	TestEqual(TEXT("한 시간상수면 63% 좁힌다"), FPaintGaugeMath::Approach(0.0f, 1.0f, 0.2f, 0.2f), 0.6321f, 1e-3f);

	// 줄어드는 쪽도 같다. 잉크는 쏠 때 줄고 쉴 때 찬다.
	TestTrue(TEXT("줄어들 때도 목표 쪽으로 간다"),
		FPaintGaugeMath::Approach(1.0f, 0.0f, 0.05f, 0.2f) < 1.0f);

	// 이게 이 함수의 핵심이다: 충분히 굴리면 목표에 정확히 닿는다.
	float Fill = 0.0f;
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		Fill = FPaintGaugeMath::Approach(Fill, 1.0f, 1.0f / 60.0f, 0.15f);
	}
	TestEqual(TEXT("계속 굴리면 목표에 정확히 닿는다"), Fill, 1.0f);

	// 이미 목표면 움직이지 않는다.
	TestEqual(TEXT("목표에 있으면 그대로"), FPaintGaugeMath::Approach(0.42f, 0.42f, 0.016f, 0.15f), 0.42f);

	return true;
}

#endif
