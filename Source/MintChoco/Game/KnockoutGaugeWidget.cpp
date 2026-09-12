#include "Game/KnockoutGaugeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#include "Game/GameGameState.h"
#include "Game/GameHudWidget.h"
#include "Game/TeamTypes.h"

/**
 * 70 %까지 칠하지 않고도 링의 모양을 볼 수 있게 한다. 이 맵에서 KO 조건은 웬만해서 나오지
 * 않으므로, 연출을 손보는 동안 실제 경기를 재현할 수는 없다. 음수면 실제 상태를 쓴다.
 */
static TAutoConsoleVariable<float> CVarKnockoutPreview(
	TEXT("mc.KnockoutPreview"),
	-1.0f,
	TEXT("0..1 로 KO 게이지를 강제로 그린다(0 이 방금 시작, 1 이 KO 직전). 음수면 실제 상태를 쓴다."),
	ECVF_Cheat);

UKnockoutGaugeWidget::UKnockoutGaugeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Font = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	Font.OutlineSettings.OutlineSize = 2;
}

AGameGameState* UKnockoutGaugeWidget::GetGameState() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetGameState<AGameGameState>() : nullptr;
}

TSharedRef<SWidget> UKnockoutGaugeWidget::RebuildWidget()
{
	// 링도 숫자도 직접 그리므로 자식 위젯이 필요 없다. 다만 그릴 넓이를 얻으려면 루트는 있어야
	// 한다. 디자인된 트리를 가진 위젯 BP로 상속했다면 그 트리를 그대로 둔다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	return Super::RebuildWidget();
}

void UKnockoutGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 경기가 시작되기도 전에 게이지가 보이면 안 된다. 첫 틱에서 다시 정해진다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UKnockoutGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AGameGameState* const State = GetGameState();
	const float Preview = CVarKnockoutPreview.GetValueOnGameThread();

	bool bPending = false;
	int32 Team = PendingTeam;

	if (Preview >= 0.0f)
	{
		// 미리보기: 남은 시간도 진행도에서 거꾸로 만들어 안쪽 숫자까지 함께 확인한다.
		bPending = true;
		Team = Teams::IsValidId(Team) ? Team : Teams::Mint;
		Progress = FMath::Clamp(Preview, 0.0f, 1.0f);
		const float Hold = State ? State->GetKnockoutHoldSeconds() : 5.0f;
		Remaining = Hold * (1.0f - Progress);
	}
	else if (State)
	{
		bPending = State->IsKnockoutPending();
		if (bPending)
		{
			Team = State->GetKnockoutTeam();
			Progress = State->GetKnockoutProgress();
			Remaining = State->GetKnockoutRemaining();
		}
	}

	SetPending(bPending, Team);
	if (!bPending)
	{
		Progress = 0.0f;
		Remaining = 0.0f;
		ShownSecond = 0;
		return;
	}

	// 경기 타이머와 같은 규칙으로 올림한다. 5.0초가 5로, 0.1초도 1로 보이므로 5·4·3·2·1이 차례로 선다.
	const int32 Second = FGameHudMath::CeilSeconds(Remaining);
	if (Second != ShownSecond)
	{
		ShownSecond = Second;
		SinceSecondChanged = 0.0f;
		BP_OnKnockoutSecond(Second, Team);
	}
	else
	{
		SinceSecondChanged += InDeltaTime;
	}
}

void UKnockoutGaugeWidget::SetPending(bool bPending, int32 Team)
{
	if (bPendingNow == bPending && PendingTeam == Team)
	{
		return;
	}

	const bool bChanged = bPendingNow != bPending;
	bPendingNow = bPending;
	PendingTeam = Team;

	if (bChanged)
	{
		// HitTestInvisible: 게이지는 보기만 하는 것이라 아래의 조작을 가로채면 안 된다.
		SetVisibility(bPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	BP_OnKnockoutChanged(bPending, Team);
}

FLinearColor UKnockoutGaugeWidget::GetRingColor() const
{
	return bTintByTeam && Teams::IsValidId(PendingTeam)
		? FLinearColor(Teams::GetDisplayColor(PendingTeam))
		: Color;
}

void UKnockoutGaugeWidget::BuildArc(const FVector2f& Center, float Fraction, TArray<FVector2f>& OutPoints) const
{
	// 슬레이트는 Y가 아래로 자라므로 12시가 -Y이고, 시계 방향은 +X 쪽으로 기운다.
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
	const float Sweep = 2.0f * UE_PI * Clamped;
	const float Direction = bClockwise ? 1.0f : -1.0f;

	// 짧은 호도 원과 같은 각 밀도로 그린다. 그러지 않으면 막 시작한 게이지가 굵은 직선 하나로 보인다.
	const int32 Count = FMath::Max(FMath::CeilToInt(static_cast<float>(Segments) * Clamped), 1);
	const float Step = Sweep / static_cast<float>(Count);

	OutPoints.Reset(Count + 1);
	for (int32 Index = 0; Index <= Count; ++Index)
	{
		const float Angle = Direction * Step * static_cast<float>(Index);
		OutPoints.Add(Center + FVector2f(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius);
	}
}

int32 UKnockoutGaugeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bPendingNow)
	{
		return Layer;
	}

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const FVector2f Center = Size * 0.5f;
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	const FLinearColor Ring = GetRingColor();

	// 도넛 안쪽을 어둡게 덮어 숫자가 배경과 겹쳐도 읽히게 한다. 구멍의 지름에 꼭 맞는 사각형은
	// 모서리가 링 밖으로 나가므로, 지름에 1/√2를 곱해 링 안에 들어가는 크기로 줄인다.
	if (HoleColor.A > 0.0f)
	{
		const float Hole = FMath::Max((Radius - Thickness * 0.5f) * 2.0f * 0.707f, 0.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(Hole, Hole), FSlateLayoutTransform(Center - FVector2f(Hole, Hole) * 0.5f)),
			FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, HoleColor);
	}

	// 채워지지 않은 나머지를 흐리게 깔아 둔다. 없으면 짧은 호가 게이지가 아니라 흘린 자국으로 보인다.
	if (TrackOpacity > UE_KINDA_SMALL_NUMBER)
	{
		TArray<FVector2f> TrackPoints;
		BuildArc(Center, 1.0f, TrackPoints);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 2, Geometry, MoveTemp(TrackPoints),
			ESlateDrawEffect::None, FLinearColor(Ring.R, Ring.G, Ring.B, Ring.A * TrackOpacity), /*bAntialias=*/true, TrackThickness);
	}

	// 차오르거나(기본) 줄어들거나. 어느 쪽이든 링이 닫히거나 사라지는 순간이 KO다.
	const float Filled = bDrain ? 1.0f - Progress : Progress;
	if (Filled > UE_KINDA_SMALL_NUMBER)
	{
		TArray<FVector2f> Points;
		BuildArc(Center, Filled, Points);

		// 슬레이트에는 블룸이 없으므로, 같은 호를 넓고 흐리게 밑에 겹쳐 빛이 번지는 것처럼 보이게 한다.
		for (int32 Halo = GlowLayers; Halo >= 1; --Halo)
		{
			const float Width = Thickness * FMath::Pow(2.0f, static_cast<float>(Halo));
			const float Alpha = FMath::Min(GlowStrength / FMath::Pow(4.0f, static_cast<float>(Halo - 1)), 1.0f);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer + 3, Geometry, Points,
				ESlateDrawEffect::None, FLinearColor(Ring.R, Ring.G, Ring.B, Ring.A * Alpha), /*bAntialias=*/true, Width);
		}

		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 4, Geometry, MoveTemp(Points),
			ESlateDrawEffect::None, Ring, /*bAntialias=*/true, Thickness);
	}

	// 도넛 안쪽의 숫자. 초가 바뀐 직후 한 번 커졌다 돌아와 초침처럼 읽힌다.
	if (ShownSecond > 0)
	{
		FSlateFontInfo Scaled = Font;
		if (TickPulse > 1.0f)
		{
			constexpr float PulseTime = 0.18f;
			const float Pulse = FMath::Clamp(1.0f - SinceSecondChanged / PulseTime, 0.0f, 1.0f);
			Scaled.Size = FMath::RoundToInt(static_cast<float>(Font.Size) * FMath::Lerp(1.0f, TickPulse, Pulse));
		}

		// 자릿수가 바뀌어도 가운데에 서도록 그릴 때마다 실제로 잰다.
		const FString Text = FString::FromInt(ShownSecond);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D Extent = Measure->Measure(Text, Scaled);
		const FVector2f TextPos = Center - FVector2f(static_cast<float>(Extent.X), static_cast<float>(Extent.Y)) * 0.5f;

		FSlateDrawElement::MakeText(OutDrawElements, Layer + 5,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TextPos)),
			Text, Scaled, ESlateDrawEffect::None, Color);
	}

	return Layer + 5;
}
