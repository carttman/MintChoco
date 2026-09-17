#include "Game/MatchResultBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/TeamLook.h"

namespace MatchResultBar
{
	/** 프레임이 길게 멈췄다 돌아와도 격돌 입자가 한 번에 멀리 튀지 않게 하는 상한. */
	constexpr float MaxEffectDeltaTime = 0.1f;

	/** 따라가는 커버리지가 이만큼 가까워지면 목표에 붙인다. 지수 보간은 목표에 닿지 않아서 합이 정확히 ClashCoverage인 값이 맞닿지 못한다. */
	constexpr float CoverageSnap = 5.0e-4f;

	const FSlateBrush& SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return Brush;
	}

	/** 프레임 길이와 상관없이 Seconds 동안 목표의 약 63%를 따라가는 보간 계수. */
	float SmoothingAlpha(float DeltaTime, float Seconds)
	{
		return Seconds > 0.0f ?
			1.0f - FMath::Exp(-DeltaTime / Seconds) :
			1.0f;
	}

	TSharedPtr<FSlateFontMeasure> GetFontMeasure()
	{
		if (!FSlateApplication::IsInitialized()) return nullptr;

		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	}
}

UMatchResultBarWidget::UMatchResultBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFont(*UWidget::GetDefaultFontName());
		KoFont = FSlateFontInfo(RobotoFont.Object, 18, FName(TEXT("Bold")));
		KoFont.OutlineSettings.OutlineSize = 2;
		KoFont.OutlineSettings.OutlineColor = FLinearColor::White;
	}

	// 위에서부터 밝은 띠, 원래 색에 가까운 띠, 어두운 띠를 겹쳐 액체에 두께감을 준다.
	const auto SetWave = [this](int32 Index, float Height, float Amplitude, float Length, float Speed, float Shade)
	{
		FMatchResultBarWave& Wave = Waves[Index];
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

	LocalSize = FVector2f(BarSize);
}

void UMatchResultBarWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (RootBox)
	{
		RootBox->SetWidthOverride(static_cast<float>(BarSize.X));
		RootBox->SetHeightOverride(static_cast<float>(BarSize.Y));
	}
	BarBrush.ImageSize = FVector2f(BarSize);
}

void UMatchResultBarWidget::SetShownCoverage(float InLeftCoverage, float InRightCoverage, int32 InKnockoutWinner)
{
	ShownCoverage = FVector2f(FMath::Clamp(InLeftCoverage, 0.0f, 1.0f), FMath::Clamp(InRightCoverage, 0.0f, 1.0f));
	KnockoutWinner = InKnockoutWinner;
}

void UMatchResultBarWidget::SetMatchRules(float InClashCoverage, float InKoLine)
{
	ClashCoverage = FMath::Max(InClashCoverage, 0.01f);
	KoLine = FMath::Clamp(InKoLine, 0.0f, 0.45f);
}

void UMatchResultBarWidget::SetBarSize(const FVector2D& InBarSize)
{
	BarSize = InBarSize;
	// 루트 SizeBox와 브러시가 크기를 따로 들고 있다. 이미 만들어진 뒤에도 맞도록 같은 경로를 탄다.
	SynchronizeProperties();
}

TSharedRef<SWidget> UMatchResultBarWidget::RebuildWidget()
{
	// 바는 자식 위젯 없이 NativePaint로 그린다. 배치할 크기만 SizeBox로 알린다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootBox"));
		WidgetTree->RootWidget = RootBox;
	}
	return Super::RebuildWidget();
}

void UMatchResultBarWidget::NativeConstruct()
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

void UMatchResultBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float DeltaTime = FMath::Max(InDeltaTime, 0.0f);
	LocalSize = FVector2f(MyGeometry.GetLocalSize());
	WaveTime += DeltaTime;

	if (IsDesignTime() && bDesignerPreview)
	{
		SetShownCoverage(DesignerLeftCoverage, DesignerRightCoverage, DesignerKnockoutWinner);
	}

	SmoothedCoverage = bHasCoverage ?
		FMath::Lerp(SmoothedCoverage, ShownCoverage, MatchResultBar::SmoothingAlpha(DeltaTime, FillSmoothingSeconds)) :
		ShownCoverage;

	if (FMath::Abs(SmoothedCoverage.X - ShownCoverage.X) < MatchResultBar::CoverageSnap)
	{
		SmoothedCoverage.X = ShownCoverage.X;
	}
	if (FMath::Abs(SmoothedCoverage.Y - ShownCoverage.Y) < MatchResultBar::CoverageSnap)
	{
		SmoothedCoverage.Y = ShownCoverage.Y;
	}
	bHasCoverage = true;

	UpdateFinish(DeltaTime);

	const FPaintBarFill ShownFill = FPaintBarMath::ComputeFill(SmoothedCoverage.X, SmoothedCoverage.Y, ClashCoverage);
	DisplayedFill = FPaintBarMath::Exaggerate(ShownFill, WinnerExaggeration * LeftFinishBlend, WinnerExaggeration * RightFinishBlend);

	UpdateClash(DeltaTime);
	UpdateMaterial();
}

bool UMatchResultBarWidget::IsLoser(bool bLeft) const
{
	// 왼쪽이 진 것은 오른쪽 팀이 이겼을 때다.
	return KnockoutWinner == (bLeft ? RightPaintId : LeftPaintId);
}

void UMatchResultBarWidget::UpdateFinish(float DeltaTime)
{
	const float Speed = 1.0f / FMath::Max(FinishBlendSeconds, 0.01f);
	LeftFinishBlend = FMath::FInterpConstantTo(LeftFinishBlend, IsLoser(/*bLeft=*/true) ? 1.0f : 0.0f, DeltaTime, Speed);
	RightFinishBlend = FMath::FInterpConstantTo(RightFinishBlend, IsLoser(/*bLeft=*/false) ? 1.0f : 0.0f, DeltaTime, Speed);
}

void UMatchResultBarWidget::UpdateClash(float DeltaTime)
{
	const bool bClashing = DisplayedFill.IsClashing() && !Teams::IsValidId(KnockoutWinner);

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
		Effect->Tick(Frame, FMath::Min(DeltaTime, MatchResultBar::MaxEffectDeltaTime));
	}
}

void UMatchResultBarWidget::UpdateMaterial()
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

	// 재질이 요구하는 파라미터지만 이 바에는 위험 점멸이 없다. State 의 앞 두 값이 늘 0 이라 쓰이지 않는다.
	Material.SetVectorParameterValue(DangerColorName, FLinearColor::Black);
	Material.SetVectorParameterValue(
		StateName,
		FLinearColor(0.0f, 0.0f, LeftFinishBlend * LoserDullAmount, RightFinishBlend * LoserDullAmount));

	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Waves)); ++Index)
	{
		const FMatchResultBarWave& Wave = Waves[Index];
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

bool UMatchResultBarWidget::EnsureMaterialInstance()
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

FLinearColor UMatchResultBarWidget::GetTeamColor(bool bLeft) const
{
	return TeamLook::GetColor(bLeft ? LeftPaintId : RightPaintId, GetWorld());
}

FLinearColor UMatchResultBarWidget::GetDulledColor(bool bLeft) const
{
	// M_UI_PaintBar의 ApplyState와 같은 식이다. 격돌 연출의 방울이 액체와 같은 색으로 보이게 한다.
	const FLinearColor Base = GetTeamColor(bLeft);
	const float Dull = GetFinishBlend(bLeft) * LoserDullAmount;
	const float Grey = Base.R * 0.2126f + Base.G * 0.7152f + Base.B * 0.0722f;

	FLinearColor Color = FMath::Lerp(Base, FLinearColor(Grey, Grey, Grey, Base.A), Dull) * (1.0f - 0.3f * Dull);
	Color.A = Base.A;
	return Color;
}

FLinearColor UMatchResultBarWidget::GetMarkColor(bool bLeft) const
{
	// 왼쪽 판정선은 오른쪽 팀이 넘어야 하는 선이라 오른쪽 팀 색이다.
	FLinearColor Hsv = GetTeamColor(!bLeft).LinearRGBToHSV();
	Hsv.B *= MarkDarken;
	return Hsv.HSVToLinearRGB();
}

float UMatchResultBarWidget::GetInnerSpan(const FVector2f& Size) const
{
	return FMath::Max(Size.X - 2.0f * ShellPadding, 1.0f);
}

float UMatchResultBarWidget::GetMarkX(const FVector2f& Size, bool bLeft) const
{
	// 왼쪽 판정선은 오른쪽 팀의 게이지가 닿는 자리라 오른쪽 끝에서 1 - KoLine 만큼 들어온 곳이다.
	const float Offset = (1.0f - KoLine) * GetInnerSpan(Size);
	return bLeft ? Size.X - ShellPadding - Offset : ShellPadding + Offset;
}

FPaintBarClashFrame UMatchResultBarWidget::MakeClashFrame() const
{
	FPaintBarClashFrame Frame;
	Frame.Contact = FVector2f(ShellPadding + DisplayedFill.Left * GetInnerSpan(LocalSize), LocalSize.Y * 0.5f);
	Frame.LiquidHeight = FMath::Max(LocalSize.Y - 2.0f * ShellPadding, 1.0f);
	Frame.Strength = ClashStrength;
	Frame.ContactVelocity = ContactVelocity;
	Frame.LeftColor = GetDulledColor(true);
	Frame.RightColor = GetDulledColor(false);
	return Frame;
}

int32 UMatchResultBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (BarMaterialInstance)
	{
		++Layer;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), &BarBrush,
			ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
	}

	// 판정선이 꺼진 규칙(KoLine 0)이면 선과 글자를 숨긴다. 세는 링은 이 바에 없다.
	const bool bLineOnBar = KoLine > 0.0f;

	++Layer;
	if (bLineOnBar)
	{
		PaintMark(AllottedGeometry, OutDrawElements, Layer, true);
		PaintMark(AllottedGeometry, OutDrawElements, Layer, false);
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
	return Layer;
}

int32 UMatchResultBarWidget::PaintMark(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const
{
	const FVector2f Size(Geometry.GetLocalSize());
	const float X = GetMarkX(Size, bLeft);
	const float Top = -MarkOverhangTop;
	const float Bottom = Size.Y + MarkOverhangBottom;
	const FVector2f RectSize(MarkThickness, Bottom - Top);

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(FVector2f(X - MarkThickness * 0.5f, Top))),
		&MatchResultBar::SolidBrush(), ESlateDrawEffect::None, GetMarkColor(bLeft));
	return LayerId;
}

int32 UMatchResultBarWidget::PaintLabel(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const
{
	const TSharedPtr<FSlateFontMeasure> Measure = MatchResultBar::GetFontMeasure();
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
