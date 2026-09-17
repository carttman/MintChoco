#include "Game/PaintBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/GameGameState.h"
#include "Game/TeamLook.h"
#include "Paint/PaintCellGrid.h"
#include "Paint/PaintSubsystem.h"

namespace PaintBarWidget
{
	/** 프레임이 길게 멈췄다 돌아와도 격돌 입자가 한 번에 멀리 튀지 않게 하는 상한. KO 시계는 실제 시간을 그대로 쓴다. */
	constexpr float MaxEffectDeltaTime = 0.1f;

	/** 따라가는 커버리지가 이만큼 가까워지면 목표에 붙인다. 지수 보간은 목표에 닿지 않아서 합이 정확히 ClashCoverage인 값이 맞닿지 못한다. */
	constexpr float CoverageSnap = 5.0e-4f;

	/** 링이 튀어나오는 시간과 나타나고 사라지는 속도(초당 알파). */
	constexpr float RingPopSeconds = 0.18f;
	constexpr float RingFadeInSpeed = 8.0f;
	constexpr float RingCancelFadeSpeed = 6.0f;
	constexpr int32 RingSegments = 48;

	const FSlateBrush& SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return Brush;
	}

	/** 모서리 반지름이 높이의 절반인 둥근 상자. 정사각형으로 그리면 원이다. */
	const FSlateBrush& DiscBrush()
	{
		static const FSlateBrush Brush = []
		{
			FSlateBrush Result;
			Result.DrawAs = ESlateBrushDrawType::RoundedBox;
			Result.TintColor = FSlateColor(FLinearColor::White);
			Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			Result.OutlineSettings.Width = 0.0f;

			return Result;
		}();

		return Brush;
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;

		return Color;
	}

	/** 프레임 길이와 상관없이 Seconds 동안 목표의 약 63%를 따라가는 보간 계수. */
	float SmoothingAlpha(float DeltaTime, float Seconds)
	{
		return Seconds > 0.0f ?
			1.0f - FMath::Exp(-DeltaTime / Seconds) :
			1.0f;
	}

	float EaseOutCubic(float Alpha)
	{
		const float Inverse = 1.0f - FMath::Clamp(Alpha, 0.0f, 1.0f);

		return 1.0f - Inverse * Inverse * Inverse;
	}

	TSharedPtr<FSlateFontMeasure> GetFontMeasure()
	{
		if (!FSlateApplication::IsInitialized()) return nullptr;

		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	}
}

UPaintBarWidget::UPaintBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFont(*UWidget::GetDefaultFontName());
		KoFont = FSlateFontInfo(RobotoFont.Object, 18, FName(TEXT("Bold")));
		KoFont.OutlineSettings.OutlineSize = 2;
		KoFont.OutlineSettings.OutlineColor = FLinearColor::White;
		RingFont = FSlateFontInfo(RobotoFont.Object, 15, FName(TEXT("Bold")));
	}

	// 위에서부터 밝은 띠, 원래 색에 가까운 띠, 어두운 띠를 겹쳐 액체에 두께감을 준다.
	const auto SetWave = [this](int32 Index, float Height, float Amplitude, float Length, float Speed, float Shade)
	{
		FPaintBarWave& Wave = Waves[Index];
		Wave.Height = Height;
		Wave.Amplitude = Amplitude;
		Wave.Length = Length;
		Wave.Speed = Speed;
		Wave.Shade = Shade;
	};
	SetWave(0, 0.18f, 1.5f, 140.0f, 18.0f, 0.14f);
	SetWave(1, 0.45f, 2.0f, 200.0f, 26.0f, -0.04f);
	SetWave(2, 0.72f, 1.5f, 110.0f, 34.0f, -0.12f);

	ClashEffects.Add(CreateDefaultSubobject<UPaintBarSparkEffect>(TEXT("SparkEffect")));

	DesignerPreview.bEnabled = true;
	LocalSize = FVector2f(BarSize);
}

void UPaintBarWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (RootBox)
	{
		RootBox->SetWidthOverride(static_cast<float>(BarSize.X));
		RootBox->SetHeightOverride(static_cast<float>(BarSize.Y));
	}
	BarBrush.ImageSize = FVector2f(BarSize);
}

void UPaintBarWidget::SetCoverageOverride(const FPaintBarPreview& InOverride)
{
	if (InOverride.bLoopDemo && !CoverageOverride.bLoopDemo)
	{
		DemoTime = 0.0f;
	}
	CoverageOverride = InOverride;
}

TSharedRef<SWidget> UPaintBarWidget::RebuildWidget()
{
	// 바는 자식 위젯 없이 NativePaint로 그린다. 배치할 크기만 SizeBox로 알리고, UMG 하위 클래스가 만든 트리는 그대로 둔다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootBox"));
		WidgetTree->RootWidget = RootBox;
	}
	return Super::RebuildWidget();
}

void UPaintBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	for (UPaintBarClashEffect* const Effect : ClashEffects)
	{
		if (Effect)
		{
			Effect->Reset();
		}
	}
}

void UPaintBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float DeltaTime = FMath::Max(InDeltaTime, 0.0f);
	LocalSize = FVector2f(MyGeometry.GetLocalSize());

	// 규칙은 GameState 것이다. 없는 곳(샘플 맵, 미리보기)에서만 Rules 의 대체값으로 로컬 시계를 돌린다.
	const AGameGameState* const GameState = FindRuleSource();

	// 경기가 끝나면 이 바는 그 순간의 그림에서 멈춘다. 물결도 게이지도 그대로다. 이어지는 KO 마무리는
	// 결과 연출이 자기 바(UMatchResultBarWidget)로 따로 그린다. 커버리지 미리보기를 켠 바는
	// FindRuleSource 가 nullptr 이라 여기 걸리지 않는다.
	if (GameState && GameState->IsMatchEnded())
	{
		// 다 센 시계에 남은 초는 없다. 링은 가득 찬 채로 남는다.
		LeftSide.RingNumber = 0;
		RightSide.RingNumber = 0;
		UpdateMaterial();
		return;
	}

	WaveTime += DeltaTime;

	const FVector2f RawCoverage = ReadCoverage(DeltaTime);
	SmoothedCoverage = bHasCoverage ?
		FMath::Lerp(SmoothedCoverage, RawCoverage, PaintBarWidget::SmoothingAlpha(DeltaTime, FillSmoothingSeconds)) :
		RawCoverage;

	if (FMath::Abs(SmoothedCoverage.X - RawCoverage.X) < PaintBarWidget::CoverageSnap)
	{
		SmoothedCoverage.X = RawCoverage.X;
	}
	if (FMath::Abs(SmoothedCoverage.Y - RawCoverage.Y) < PaintBarWidget::CoverageSnap)
	{
		SmoothedCoverage.Y = RawCoverage.Y;
	}
	bHasCoverage = true;

	// 판정은 복제된 값 그대로 하고, 그림은 따라가는 값으로 그린다. 따라가는 값도 같은 식을 거치므로 두 게이지는 정확히 맞닿는다.
	const float ClashCoverage = GameState ? GameState->GetClashCoverage() : Rules.ClashCoverage;
	const float KoHoldSeconds = GameState ? GameState->GetKnockoutHoldSeconds() : Rules.PreviewKoHoldSeconds;
	KoLine = FMath::Clamp(GameState ? GameState->GetKnockoutLine() : Rules.KoLine, 0.0f, 1.0f);

	const FPaintBarFill RuleFill = FPaintBarMath::ComputeFill(RawCoverage.X, RawCoverage.Y, ClashCoverage);
	UpdateSide(LeftSide, RuleFill.Right, MakeKoStatus(LeftSide, GameState, RightPaintId, RuleFill.Right, KoHoldSeconds, DeltaTime), DeltaTime);
	UpdateSide(RightSide, RuleFill.Left, MakeKoStatus(RightSide, GameState, LeftPaintId, RuleFill.Left, KoHoldSeconds, DeltaTime), DeltaTime);

	const FPaintBarFill ShownFill = FPaintBarMath::ComputeFill(SmoothedCoverage.X, SmoothedCoverage.Y, ClashCoverage);
	DisplayedFill = FPaintBarMath::Exaggerate(ShownFill, KoExaggeration * LeftSide.KoBlend, KoExaggeration * RightSide.KoBlend);

	UpdateClash(DeltaTime);
	UpdateMaterial();
}

const FPaintBarPreview* UPaintBarWidget::FindCoveragePreview() const
{
	if (IsDesignTime())
	{
		return DesignerPreview.bEnabled ? &DesignerPreview : nullptr;
	}
	return CoverageOverride.bEnabled ? &CoverageOverride : nullptr;
}

FVector2f UPaintBarWidget::ReadCoverage(float DeltaTime)
{
	const FPaintBarPreview* const Preview = FindCoveragePreview();
	if (Preview)
	{
		if (Preview->bLoopDemo)
		{
			DemoTime += DeltaTime;
			return FPaintBarMath::DemoCoverage(DemoTime, Preview->DemoPeriod);
		}
		return FVector2f(Preview->LeftCoverage, Preview->RightCoverage);
	}

	const UWorld* const World = IsDesignTime() ? nullptr : GetWorld();
	if (!World)
	{
		return FVector2f::ZeroVector;
	}

	// 점수는 서버 것이다. 클라이언트 그리드는 자기가 그린 스플랫만 비추므로 GameState가 있으면 그쪽을 읽는다.
	if (const AGameGameState* const GameState = World->GetGameState<AGameGameState>())
	{
		return CoverageOf(GameState->GetWorldCoverage());
	}
	if (const UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>())
	{
		return CoverageOf(Paint->GetWorldCoverage());
	}
	return FVector2f::ZeroVector;
}

FVector2f UPaintBarWidget::CoverageOf(const FPaintCoverage& Coverage) const
{
	if (Coverage.TotalArea <= 0.0f)
	{
		return FVector2f::ZeroVector;
	}
	const uint8 Left = static_cast<uint8>(FMath::Clamp(LeftPaintId, 0, PaintIdCount - 1));
	const uint8 Right = static_cast<uint8>(FMath::Clamp(RightPaintId, 0, PaintIdCount - 1));
	return FVector2f(Coverage.GetFraction(Left), Coverage.GetFraction(Right));
}

const AGameGameState* UPaintBarWidget::FindRuleSource() const
{
	if (IsDesignTime() || CoverageOverride.bEnabled)
	{
		return nullptr;
	}
	const UWorld* const World = GetWorld();
	return World ? World->GetGameState<AGameGameState>() : nullptr;
}

UPaintBarWidget::FKoStatus UPaintBarWidget::MakeKoStatus(FSideState& Side, const AGameGameState* GameState, int32 OpponentPaintId,
	float OpponentFill, float KoHoldSeconds, float DeltaTime) const
{
	FKoStatus Status;
	if (GameState)
	{
		// 서버가 센 시각을 그대로 보여 준다. 링이 차는 속도와 숫자가 모든 머신에서 같다. KO 가 나는
		// 순간 경기가 끝나 이 바는 NativeTick 에서 멈추므로, 여기서 KO 마무리가 걸릴 일은 없다.
		Status.bCounting = GameState->IsKnockoutPending() && GameState->GetKnockoutTeam() == OpponentPaintId;
		Status.Progress = Status.bCounting ? GameState->GetKnockoutProgress() : 0.0f;
		Status.SecondsLeft = Status.bCounting ? FMath::CeilToInt(GameState->GetKnockoutRemaining()) : 0;
		return Status;
	}

	// 미리보기가 판정선과 무관하게 이긴 팀을 들려 보낸다. 한쪽의 플래그는 '상대가 이겼다'를 뜻하므로
	// 진 쪽 액체가 탁해지고 이긴 쪽 게이지가 더 밀린다.
	if (const FPaintBarPreview* const Preview = FindCoveragePreview())
	{
		if (Teams::IsValidId(Preview->ForcedKnockoutTeam))
		{
			Status.bKnockedOut = Preview->ForcedKnockoutTeam == OpponentPaintId;
			Side.Clock = FPaintKoClock();
			return Status;
		}
	}

	Side.Clock.Advance(KoLine > 0.0f && FPaintBarMath::IsPastKoLine(OpponentFill, KoLine), DeltaTime, KoHoldSeconds);
	Status.bCounting = Side.Clock.IsCounting();
	Status.Progress = Side.Clock.GetProgress(KoHoldSeconds);
	Status.SecondsLeft = Side.Clock.GetSecondsLeft(KoHoldSeconds);
	Status.bKnockedOut = Side.Clock.bKnockedOut;
	return Status;
}

void UPaintBarWidget::UpdateSide(FSideState& Side, float OpponentFill, const FKoStatus& Ko, float DeltaTime) const
{
	using namespace PaintBarWidget;

	const bool bKnockedOut = Ko.bKnockedOut;
	Side.bKnockedOut = bKnockedOut;

	// 선을 넘긴 뒤 카운트다운 중에도 점멸하고, KO가 나면 점멸 대신 탁해진다.
	const bool bDanger = KoLine > 0.0f && FPaintBarMath::IsInDanger(OpponentFill, KoLine, Rules.DangerMargin) && !bKnockedOut;
	Side.DangerEnvelope = FMath::FInterpConstantTo(Side.DangerEnvelope, bDanger ? 1.0f : 0.0f, DeltaTime, 1.0f / FMath::Max(DangerFadeSeconds, 0.01f));
	Side.DangerPhase = Side.DangerEnvelope > 0.0f ? Side.DangerPhase + DeltaTime / FMath::Max(DangerPulsePeriod, 0.05f) : 0.0f;

	const float KoSpeed = 1.0f / FMath::Max(KoBlendSeconds, 0.01f);
	Side.KoBlend = FMath::FInterpConstantTo(Side.KoBlend, bKnockedOut ? 1.0f : 0.0f, DeltaTime, KoSpeed);

	if (Ko.bCounting && !bKnockedOut)
	{
		Side.RingAge += DeltaTime;
		Side.RingAlpha = FMath::FInterpConstantTo(Side.RingAlpha, 1.0f, DeltaTime, RingFadeInSpeed);
		Side.RingProgress = Ko.Progress;
		Side.RingNumber = Ko.SecondsLeft;
		return;
	}

	// KO가 나면 가득 찬 링이 KO 연출과 함께 사라지고, 선에서 빠져나오면 그 자리에서 빨리 사라진다.
	if (bKnockedOut)
	{
		Side.RingProgress = 1.0f;
		Side.RingNumber = 0;
	}
	Side.RingAlpha = FMath::FInterpConstantTo(Side.RingAlpha, 0.0f, DeltaTime, bKnockedOut ? KoSpeed : RingCancelFadeSpeed);
	if (Side.RingAlpha <= 0.0f)
	{
		Side.RingAge = 0.0f;
	}
}

void UPaintBarWidget::UpdateClash(float DeltaTime)
{
	const bool bKnockedOut = LeftSide.bKnockedOut || RightSide.bKnockedOut;
	const bool bClashing = DisplayedFill.IsClashing() && !bKnockedOut;

	const float FadeSeconds = bClashing ? ClashFadeInSeconds : ClashFadeOutSeconds;
	ClashStrength = FMath::FInterpConstantTo(ClashStrength, bClashing ? 1.0f : 0.0f, DeltaTime, 1.0f / FMath::Max(FadeSeconds, 0.01f));

	const float ContactX = ShellPadding + DisplayedFill.Left * GetInnerSpan(LocalSize);
	if (DeltaTime > 0.0f)
	{
		const float Instant = bClashing && bWasClashing ? (ContactX - LastContactX) / DeltaTime : 0.0f;
		ContactVelocity = FMath::Lerp(ContactVelocity, Instant, 0.3f);
	}
	LastContactX = ContactX;

	const FPaintBarClashFrame Frame = MakeClashFrame();
	const bool bBegan = bClashing && !bWasClashing;
	bWasClashing = bClashing;

	for (UPaintBarClashEffect* const Effect : ClashEffects)
	{
		if (!Effect) continue;

		if (bBegan)
		{
			Effect->OnClashBegin(Frame);
		}
		Effect->Tick(Frame, FMath::Min(DeltaTime, PaintBarWidget::MaxEffectDeltaTime));
	}
}

void UPaintBarWidget::UpdateMaterial()
{
	if (!EnsureMaterialInstance()) return;

	float Turbulence = 0.0f;
	float FoamWidth = 0.0f;
	for (const UPaintBarClashEffect* const Effect : ClashEffects)
	{
		if (Effect)
		{
			Turbulence = FMath::Max(Turbulence, Effect->GetBoundaryTurbulence());
			FoamWidth = FMath::Max(FoamWidth, Effect->GetFoamWidth());
		}
	}

	// M_UI_PaintBar의 VectorParameter 이름. 재질의 Custom 노드 입력과 1:1이다.
	static const FName FrameName(TEXT("Frame"));
	static const FName FillName(TEXT("Fill"));
	static const FName LeftColorName(TEXT("LeftColor"));
	static const FName RightColorName(TEXT("RightColor"));
	static const FName ShellColorName(TEXT("ShellColor"));
	static const FName DangerColorName(TEXT("DangerColor"));
	static const FName StateName(TEXT("State"));
	static const FName WaveNames[] = {
		FName(TEXT("WaveA")),
		FName(TEXT("WaveB")),
		FName(TEXT("WaveC"))
	};
	static const FName WaveShadeName(TEXT("WaveShade"));
	static const FName FrontName(TEXT("Front"));

	UMaterialInstanceDynamic& Material = *BarMaterialInstance;
	Material.SetVectorParameterValue(FrameName, FLinearColor(LocalSize.X, LocalSize.Y, ShellPadding, WaveTime));
	Material.SetVectorParameterValue(FillName, FLinearColor(DisplayedFill.Left, DisplayedFill.Right, Turbulence, FoamWidth));
	Material.SetVectorParameterValue(LeftColorName, GetTeamColor(true));
	Material.SetVectorParameterValue(RightColorName, GetTeamColor(false));
	Material.SetVectorParameterValue(ShellColorName, ShellColor);
	Material.SetVectorParameterValue(DangerColorName, DangerColor);
	Material.SetVectorParameterValue(
		StateName,
		FLinearColor(GetDanger(LeftSide), GetDanger(RightSide), LeftSide.KoBlend * KoDullAmount, RightSide.KoBlend * KoDullAmount));

	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Waves)); ++Index)
	{
		const FPaintBarWave& Wave = Waves[Index];
		Material.SetVectorParameterValue(
			WaveNames[Index],
			FLinearColor(Wave.Height, Wave.Amplitude, Wave.Length, Wave.Speed));
	}
	Material.SetVectorParameterValue(
		WaveShadeName,
		FLinearColor(Waves[0].Shade, Waves[1].Shade, Waves[2].Shade, TopLayerOpacity));
	Material.SetVectorParameterValue(
		FrontName,
		FLinearColor(FrontAmplitude, FrontLength, FrontSpeed, 0.0f));
}

bool UPaintBarWidget::EnsureMaterialInstance()
{
	if (!BarMaterial)
	{
		BarMaterialInstance = nullptr;
		BarBrush.SetResourceObject(nullptr);
		return false;
	}

	if (!BarMaterialInstance || BarMaterialInstance->Parent != BarMaterial)
	{
		BarMaterialInstance = UMaterialInstanceDynamic::Create(BarMaterial, this);
		BarBrush.SetResourceObject(BarMaterialInstance);
		BarBrush.DrawAs = ESlateBrushDrawType::Image;
		BarBrush.ImageSize = FVector2f(BarSize);
	}
	return true;
}

float UPaintBarWidget::GetDanger(const FSideState& Side) const
{
	return DangerStrength * Side.DangerEnvelope * FPaintBarMath::Pulse(Side.DangerPhase);
}

FLinearColor UPaintBarWidget::GetDisplayedColor(const FLinearColor& Base, const FSideState& Side) const
{
	// M_UI_PaintBar의 ApplyState와 같은 식이다. 격돌 연출의 방울이 액체와 같은 색으로 보이게 한다.
	const float Dull = Side.KoBlend * KoDullAmount;
	const float Grey = Base.R * 0.2126f + Base.G * 0.7152f + Base.B * 0.0722f;
	FLinearColor Color = FMath::Lerp(Base, FLinearColor(Grey, Grey, Grey, Base.A), Dull) * (1.0f - 0.3f * Dull);
	Color = FMath::Lerp(Color, DangerColor, GetDanger(Side));
	Color.A = Base.A;
	return Color;
}

FLinearColor UPaintBarWidget::GetTeamColor(bool bLeft) const
{
	return TeamLook::GetColor(bLeft ? LeftPaintId : RightPaintId, GetWorld());
}

FLinearColor UPaintBarWidget::GetMarkColor(bool bLeft) const
{
	// 왼쪽 판정선은 오른쪽 팀이 넘어야 하는 선이라 오른쪽 팀 색이다.
	FLinearColor Hsv = GetTeamColor(!bLeft).LinearRGBToHSV();
	Hsv.B *= MarkDarken;
	return Hsv.HSVToLinearRGB();
}

float UPaintBarWidget::GetInnerSpan(const FVector2f& Size) const
{
	return FMath::Max(Size.X - 2.0f * ShellPadding, 1.0f);
}

float UPaintBarWidget::GetMarkX(const FVector2f& Size, bool bLeft) const
{
	// 왼쪽 판정선은 오른쪽 팀의 게이지가 닿는 자리라 오른쪽 끝에서 1 - KoLine 만큼 들어온 곳이다.
	const float Offset = (1.0f - KoLine) * GetInnerSpan(Size);
	return bLeft ? Size.X - ShellPadding - Offset : ShellPadding + Offset;
}

FPaintBarClashFrame UPaintBarWidget::MakeClashFrame() const
{
	FPaintBarClashFrame Frame;
	Frame.Contact = FVector2f(ShellPadding + DisplayedFill.Left * GetInnerSpan(LocalSize), LocalSize.Y * 0.5f);
	Frame.LiquidHeight = FMath::Max(LocalSize.Y - 2.0f * ShellPadding, 1.0f);
	Frame.Strength = ClashStrength;
	Frame.ContactVelocity = ContactVelocity;
	Frame.LeftColor = GetDisplayedColor(GetTeamColor(true), LeftSide);
	Frame.RightColor = GetDisplayedColor(GetTeamColor(false), RightSide);
	return Frame;
}

int32 UPaintBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (BarMaterialInstance)
	{
		++Layer;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), &BarBrush,
			ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
	}

	// KO 가 꺼진 규칙(KoLine 0)이면 선·글자·링을 모두 숨긴다.
	const bool bLineOnBar = KoLine > 0.0f;

	++Layer;
	if (bLineOnBar)
	{
		PaintMark(AllottedGeometry, OutDrawElements, Layer, true, LeftSide);
		PaintMark(AllottedGeometry, OutDrawElements, Layer, false, RightSide);
	}

	const FPaintBarClashFrame Frame = MakeClashFrame();
	int32 EffectTop = Layer;
	for (const UPaintBarClashEffect* const Effect : ClashEffects)
	{
		if (Effect)
		{
			EffectTop = FMath::Max(EffectTop, Effect->Paint(Frame, AllottedGeometry, OutDrawElements, Layer));
		}
	}

	Layer = EffectTop + 1;
	if (!bLineOnBar)
	{
		return Layer;
	}
	PaintLabel(AllottedGeometry, OutDrawElements, Layer, true);
	PaintLabel(AllottedGeometry, OutDrawElements, Layer, false);

	++Layer;
	return FMath::Max(
		PaintRing(AllottedGeometry, OutDrawElements, Layer, true, LeftSide),
		PaintRing(AllottedGeometry, OutDrawElements, Layer, false, RightSide));
}

int32 UPaintBarWidget::PaintMark(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft, const FSideState& Side) const
{
	const FVector2f Size(Geometry.GetLocalSize());
	const float X = GetMarkX(Size, bLeft);

	// 링이 뜨면 선이 링 위끝까지 내려와 둘이 이어진다.
	const float Top = -MarkOverhangTop;
	const float Bottom = Size.Y + FMath::Lerp(MarkOverhangBottom, RingGap, Side.RingAlpha);
	const FVector2f RectSize(MarkThickness, Bottom - Top);

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(FVector2f(X - MarkThickness * 0.5f, Top))),
		&PaintBarWidget::SolidBrush(), ESlateDrawEffect::None, GetMarkColor(bLeft));
	return LayerId;
}

int32 UPaintBarWidget::PaintLabel(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const
{
	const TSharedPtr<FSlateFontMeasure> Measure = PaintBarWidget::GetFontMeasure();
	if (KoText.IsEmpty() || !Measure.IsValid())
	{
		return LayerId;
	}

	const FVector2f Size(Geometry.GetLocalSize());
	const FVector2f TextSize(Measure->Measure(KoText, KoFont));
	const FVector2f TopLeft(GetMarkX(Size, bLeft) - TextSize.X * 0.5f, -MarkOverhangTop - LabelGap - TextSize.Y);

	FSlateDrawElement::MakeText(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TopLeft)),
		KoText, KoFont, ESlateDrawEffect::None, GetMarkColor(bLeft));
	return LayerId;
}

int32 UPaintBarWidget::PaintRing(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft, const FSideState& Side) const
{
	using namespace PaintBarWidget;

	if (Side.RingAlpha <= 0.0f)
	{
		return LayerId;
	}

	const FVector2f Size(Geometry.GetLocalSize());
	const float Alpha = Side.RingAlpha;
	const float Radius = RingRadius * FMath::Lerp(0.6f, 1.0f, EaseOutCubic(Side.RingAge / RingPopSeconds));
	const float DiscRadius = Radius + RingThickness * 0.5f + 1.5f;
	const FVector2f Center(GetMarkX(Size, bLeft), Size.Y + RingGap + DiscRadius);
	const FLinearColor MarkColor = GetMarkColor(bLeft);

	const FVector2f DiscSize(DiscRadius * 2.0f, DiscRadius * 2.0f);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(DiscSize, FSlateLayoutTransform(Center - DiscSize * 0.5f)),
		&DiscBrush(), ESlateDrawEffect::None, WithAlpha(RingBackgroundColor, Alpha));

	// 12시에서 시계 방향으로 찬다. 화면 좌표는 y가 아래라 각도가 커지면 시계 방향이다.
	const auto MakeArc = [&Center, Radius](float Fraction, int32 Segments)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = UE_TWO_PI * Fraction * static_cast<float>(Index) / static_cast<float>(Segments) - UE_HALF_PI;
			Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Points;
	};

	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), MakeArc(1.0f, RingSegments),
		ESlateDrawEffect::None, WithAlpha(RingTrackColor, Alpha), /*bAntialias=*/true, RingThickness);

	if (Side.RingProgress > 0.0f)
	{
		const int32 ArcSegments = FMath::Max(FMath::CeilToInt(RingSegments * Side.RingProgress), 2);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, Geometry.ToPaintGeometry(), MakeArc(Side.RingProgress, ArcSegments),
			ESlateDrawEffect::None, WithAlpha(MarkColor, Alpha), /*bAntialias=*/true, RingThickness);
	}

	const TSharedPtr<FSlateFontMeasure> Measure = GetFontMeasure();
	if (Side.RingNumber > 0 && Measure.IsValid())
	{
		const FText Number = FText::AsNumber(Side.RingNumber);
		const FVector2f TextSize(Measure->Measure(Number, RingFont));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3,
			Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Center - TextSize * 0.5f)),
			Number, RingFont, ESlateDrawEffect::None, WithAlpha(MarkColor, Alpha));
	}
	return LayerId + 3;
}
