#include "Game/PaintGaugeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"

#include "Game/TeamLook.h"
#include "Game/Unit.h"
#include "Ink/InkTankComponent.h"

// ---------------------------------------------------------------- FPaintGaugeMath

float FPaintGaugeMath::Approach(float Current, float Target, float DeltaTime, float SmoothingSeconds)
{
	if (SmoothingSeconds <= 0.0f || DeltaTime <= 0.0f)
	{
		return Target;
	}

	const float Alpha = 1.0f - FMath::Exp(-DeltaTime / SmoothingSeconds);
	const float Next = FMath::Lerp(Current, Target, Alpha);

	// 지수 보간은 목표에 닿지 않는다. 가득 찬 잉크가 영원히 조금 모자라 보이지 않게 붙인다.
	constexpr float Snap = 1.0e-4f;
	return FMath::IsNearlyEqual(Next, Target, Snap) ? Target : Next;
}

// ---------------------------------------------------------------- UPaintGaugeWidget

UPaintGaugeWidget::UPaintGaugeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 커버리지 바와 같은 결의 기본값. 초승달이 좁아 진폭과 파장을 그만큼 줄였다.
	//
	// Shade는 셋 다 0이다: 액체는 한 가지 색이고 안쪽에 띠가 지지 않는다. 초승달이 좁아 바에서처럼
	// 겹을 나누면 색이 갈라진 것으로만 보였다. Waves[0]은 여전히 액체의 윗면을 물결 모양으로 자르므로
	// (WaveShade.w = TopLayerOpacity가 0) 물결은 그대로 남는다.
	//
	// Waves[1]과 Waves[2]는 이제 아무 일도 하지 않는다. 구조를 남겨 둔 것은 띠를 다시 넣고 싶을 때
	// Shade만 올리면 되게 하려는 것이다 — 지우면 머티리얼의 Liquid()까지 고쳐야 한다.
	Waves[0].Height = 0.0f;
	Waves[0].Amplitude = 1.6f;
	Waves[0].Length = 60.0f;
	Waves[0].Speed = 14.0f;
	Waves[0].Shade = 0.0f;

	Waves[1].Height = 0.35f;
	Waves[1].Amplitude = 1.2f;
	Waves[1].Length = 46.0f;
	Waves[1].Speed = -10.0f;
	Waves[1].Shade = 0.0f;

	Waves[2].Height = 0.7f;
	Waves[2].Amplitude = 0.9f;
	Waves[2].Length = 38.0f;
	Waves[2].Speed = 7.0f;
	Waves[2].Shade = 0.0f;
}

void UPaintGaugeWidget::SetFillOverride(bool bEnabled, float Fill)
{
	bFillOverrideEnabled = bEnabled;
	FillOverride = FPaintGaugeMath::FillFromInk(Fill);
}

TSharedRef<SWidget> UPaintGaugeWidget::RebuildWidget()
{
	// 자식 위젯 없이 NativePaint로 그린다. 배치할 크기만 SizeBox로 알린다(커버리지 바와 같은 방식).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootBox"));
		WidgetTree->RootWidget = RootBox;
	}
	return Super::RebuildWidget();
}

void UPaintGaugeWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	GaugeBrush.ImageSize = FVector2f(GaugeSize);

	// 위젯 디자이너는 틱을 돌리지 않는다. 여기서 한 번 채워 두지 않으면 머티리얼 인스턴스가
	// 만들어지지 않아 NativePaint가 그릴 것이 없고, 값을 바꿔도 미리보기가 그대로다.
	if (IsPreview())
	{
		DisplayedFill = FPaintGaugeMath::FillFromInk(DesignerFill);
		bHasFill = true;
	}
	UpdateMaterial();
}

void UPaintGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 첫 틱이 오기 전에도 브러시가 있어야 한 프레임 비어 보이지 않는다.
	UpdateMaterial();
}

void UPaintGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	WaveTime += InDeltaTime;

	const float Ink = ReadInk();
	if (Ink < 0.0f)
	{
		// 아직 내 유닛이 없다(로딩, 관전). 지금 값을 그대로 두고 기다린다.
		UpdateMaterial();
		return;
	}

	const float Target = FPaintGaugeMath::FillFromInk(Ink);
	// 처음 읽은 값은 보간하지 않는다. 안 그러면 판이 시작될 때마다 게이지가 0에서 차오른다.
	DisplayedFill = bHasFill ? FPaintGaugeMath::Approach(DisplayedFill, Target, InDeltaTime, FillSmoothingSeconds) : Target;
	bHasFill = true;

	UpdateMaterial();
}

bool UPaintGaugeWidget::IsPreview() const
{
	// 위젯 디자이너의 미리보기 월드에는 폰이 없다. IsDesignTime()만 보면 남의 위젯 안에 들어간
	// 이 게이지(WBP_GameHUD의 미리보기)가 빈 채로 남아, 초승달 좌표를 눈으로 맞출 수가 없다.
	const UWorld* const World = GetWorld();
	return IsDesignTime() || (World && World->WorldType == EWorldType::EditorPreview);
}

float UPaintGaugeWidget::ReadInk() const
{
	if (IsPreview())
	{
		return DesignerFill;
	}
	if (bFillOverrideEnabled)
	{
		return FillOverride;
	}

	const AUnit* const Unit = Cast<AUnit>(GetOwningPlayerPawn());
	const UInkTankComponent* const Tank = Unit ? Unit->GetInkTank() : nullptr;
	return Tank ? Tank->GetInk() : -1.0f;
}

FLinearColor UPaintGaugeWidget::GetLiquidColor() const
{
	if (!bUseTeamColor || IsPreview())
	{
		return LiquidColor;
	}

	// 팀이 아직 없으면(관전, 팀을 고르기 전) TeamLook은 중립 회색을 준다. 그럴 때는 정한 색이 낫다.
	const AUnit* const Unit = Cast<AUnit>(GetOwningPlayerPawn());
	if (!Unit || !Teams::IsValidId(Unit->GetTeam()))
	{
		return LiquidColor;
	}
	return TeamLook::GetColor(Unit->GetPaintId(), GetWorld());
}

bool UPaintGaugeWidget::EnsureMaterialInstance()
{
	if (!GaugeMaterial)
	{
		GaugeMaterialInstance = nullptr;
		GaugeBrush.SetResourceObject(nullptr);
		return false;
	}

	if (!GaugeMaterialInstance || GaugeMaterialInstance->Parent != GaugeMaterial)
	{
		GaugeMaterialInstance = UMaterialInstanceDynamic::Create(GaugeMaterial, this);
		GaugeBrush.SetResourceObject(GaugeMaterialInstance);
		GaugeBrush.DrawAs = ESlateBrushDrawType::Image;
		GaugeBrush.ImageSize = FVector2f(GaugeSize);
	}
	return true;
}

void UPaintGaugeWidget::UpdateMaterial()
{
	if (!EnsureMaterialInstance()) return;

	// M_UI_PaintGauge의 VectorParameter 이름. 재질의 Custom 노드 입력과 1:1이다.
	static const FName FrameName(TEXT("Frame"));
	static const FName FillName(TEXT("Fill"));
	static const FName OuterName(TEXT("Outer"));
	static const FName InnerName(TEXT("Inner"));
	static const FName LiquidColorName(TEXT("LiquidColor"));
	static const FName WaveNames[] = {
		FName(TEXT("WaveA")),
		FName(TEXT("WaveB")),
		FName(TEXT("WaveC"))
	};
	static const FName WaveShadeName(TEXT("WaveShade"));

	UMaterialInstanceDynamic& Material = *GaugeMaterialInstance;
	Material.SetVectorParameterValue(FrameName, FLinearColor(GaugeSize.X, GaugeSize.Y, GuardMargin, WaveTime));
	Material.SetVectorParameterValue(FillName, FLinearColor(DisplayedFill, 0.0f, 0.0f, 0.0f));
	Material.SetVectorParameterValue(OuterName, FLinearColor(OuterCenter.X, OuterCenter.Y, OuterRadius, 0.0f));
	Material.SetVectorParameterValue(InnerName, FLinearColor(InnerCenter.X, InnerCenter.Y, InnerRadius, 0.0f));
	Material.SetVectorParameterValue(LiquidColorName, GetLiquidColor());

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
}

int32 UPaintGaugeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (GaugeMaterialInstance)
	{
		++Layer;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), &GaugeBrush,
			ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
	}

	return Layer;
}
