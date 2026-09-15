#include "Weapons/PaintCrosshairWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

#include "Weapons/PaintWeaponComponent.h"

/**
 * 상태를 강제해 모양만 확인한다: 0 기본, 1 사격, 2 마커(중앙에서 고정 오프셋). 음수면 실제
 * 무기 상태를 읽는다(출시 동작).
 */
static TAutoConsoleVariable<int32> CVarCrosshairPreview(
	TEXT("mc.CrosshairPreview"),
	-1,
	TEXT("크로스헤어 상태를 강제한다: 0 기본, 1 사격, 2 예상 탄착 마커. 음수면 실제 상태."),
	ECVF_Cheat);

namespace
{
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

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Alpha);
	}

	/** 미리보기 2번에서 마커를 놓는 곳: 중앙에서 오른쪽 위로. */
	const FVector2f PreviewMarkerOffset(90.0f, -70.0f);
}

// ---------------------------------------------------------------- FPaintCrosshairMath

bool FPaintCrosshairMath::ImpactFallsShort(const FVector& ViewOrigin, const FVector& ViewDirection, const FVector& AimPoint, const FVector& Impact, float ShortfallCm)
{
	const double AimDepth = FVector::DotProduct(AimPoint - ViewOrigin, ViewDirection);
	const double ImpactDepth = FVector::DotProduct(Impact - ViewOrigin, ViewDirection);
	return ImpactDepth < AimDepth - static_cast<double>(ShortfallCm);
}

float FPaintCrosshairMath::CenterRadius(float BaseRadius, float FiringScale, float FiringBlend)
{
	return BaseRadius * FMath::Lerp(1.0f, FiringScale, FMath::Clamp(FiringBlend, 0.0f, 1.0f));
}

float FPaintCrosshairMath::CenterOpacity(float IdleOpacity, float FiringOpacity, float FiringBlend, float MarkerBlend, float DimmedOpacity)
{
	const float Opacity = FMath::Lerp(IdleOpacity, FiringOpacity, FMath::Clamp(FiringBlend, 0.0f, 1.0f));
	return Opacity * FMath::Lerp(1.0f, DimmedOpacity, FMath::Clamp(MarkerBlend, 0.0f, 1.0f));
}

// ---------------------------------------------------------------- UPaintCrosshairWidget

TSharedRef<SWidget> UPaintCrosshairWidget::RebuildWidget()
{
	// 자식 위젯은 없고, 그릴 중앙을 줄 루트만 있으면 된다. UMG 자식 클래스는 제 트리를 그대로 쓴다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	return Super::RebuildWidget();
}

void UPaintCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPaintCrosshairWidget::NativeDestruct()
{
	SetWeapon(nullptr);
	Super::NativeDestruct();
}

void UPaintCrosshairWidget::SetWeapon(UPaintWeaponComponent* Weapon)
{
	if (BoundWeapon.Get() == Weapon)
	{
		return;
	}
	if (UPaintWeaponComponent* const Old = BoundWeapon.Get())
	{
		Old->OnFired.RemoveDynamic(this, &UPaintCrosshairWidget::HandleWeaponFired);
	}
	BoundWeapon = Weapon;
	// 다른 무기의 발사 기록을 이어받지 않는다.
	LastFiredTime = -1.0;
	if (Weapon)
	{
		Weapon->OnFired.AddUniqueDynamic(this, &UPaintCrosshairWidget::HandleWeaponFired);
	}
}

void UPaintCrosshairWidget::SetShown(bool bShown)
{
	bWantsShown = bShown;
}

void UPaintCrosshairWidget::HandleWeaponFired(int32 Seed)
{
	if (const UWorld* const World = GetWorld())
	{
		LastFiredTime = World->GetTimeSeconds();
	}
}

int32 UPaintCrosshairWidget::GetPreviewState()
{
	return CVarCrosshairPreview.GetValueOnGameThread();
}

void UPaintCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const UPaintWeaponComponent* const Weapon = BoundWeapon.Get();
	const int32 Preview = GetPreviewState();

	bool bFiring = false;
	bool bShowMarker = false;
	FVector2f MarkerTarget = FVector2f::ZeroVector;

	if (Preview >= 0)
	{
		bFiring = Preview == 1;
		bShowMarker = Preview == 2;
		MarkerTarget = MyGeometry.GetLocalSize() * 0.5f + PreviewMarkerOffset;
	}
	else if (Weapon)
	{
		bFiring = Weapon->IsTriggerHeld() || Weapon->IsAiming()
			|| (LastFiredTime >= 0.0 && Now - LastFiredTime <= FireFlashSeconds);

		FVector ViewOrigin;
		FVector ViewDirection;
		FVector AimPoint;
		FVector Impact;
		if (Weapon->PredictNextImpact(ViewOrigin, ViewDirection, AimPoint, Impact)
			&& FPaintCrosshairMath::ImpactFallsShort(ViewOrigin, ViewDirection, AimPoint, Impact, MarkerShortfallCm))
		{
			// 풀스크린 슬롯에 놓였다는 전제: 뷰포트 위젯 좌표가 곧 이 위젯의 로컬 좌표다.
			FVector2D Projected;
			if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Impact, Projected, /*bPlayerViewportRelative=*/false))
			{
				bShowMarker = true;
				MarkerTarget = FVector2f(Projected);
			}
		}
	}

	FiringBlend = FMath::FInterpTo(FiringBlend, bFiring ? 1.0f : 0.0f, InDeltaTime, FiringBlendSpeed);
	Presence = FMath::FInterpTo(Presence, bWantsShown ? 1.0f : 0.0f, InDeltaTime, PresenceSpeed);

	if (bShowMarker)
	{
		// 막 나타날 때는 제자리에, 보이는 동안은 부드럽게 따라간다.
		MarkerLocal = MarkerBlend <= UE_KINDA_SMALL_NUMBER
			? MarkerTarget
			: FVector2f(FMath::Vector2DInterpTo(FVector2D(MarkerLocal), FVector2D(MarkerTarget), InDeltaTime, MarkerInterpSpeed));
	}
	MarkerBlend = FMath::FInterpTo(MarkerBlend, bShowMarker ? 1.0f : 0.0f, InDeltaTime, MarkerBlendSpeed);
}

void UPaintCrosshairWidget::BuildCircle(const FVector2f& Center, float Radius, TArray<FVector2f>& OutPoints) const
{
	const int32 Count = FMath::Max(Segments, 8);
	OutPoints.Reset(Count + 1);
	for (int32 Index = 0; Index <= Count; ++Index)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Count);
		OutPoints.Add(Center + FVector2f(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius);
	}
}

void UPaintCrosshairWidget::DrawRing(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& AllottedGeometry,
	const FVector2f& Center, float Radius, float Thickness, float DotRadius, const FLinearColor& Color, float Opacity, int32 GlowLayers, float GlowAlpha) const
{
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	TArray<FVector2f> Points;
	BuildCircle(Center, Radius, Points);

	// Slate 에는 블룸이 없어서, 같은 링을 더 굵고 더 옅게 아래에 깐다(차지 링과 같은 방식).
	for (int32 Halo = GlowLayers; Halo >= 1; --Halo)
	{
		const float Width = Thickness * FMath::Pow(2.0f, static_cast<float>(Halo));
		const float Alpha = FMath::Min(GlowAlpha / FMath::Pow(4.0f, static_cast<float>(Halo - 1)), 1.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry, Points,
			ESlateDrawEffect::None, WithAlpha(Color, Alpha * Opacity), /*bAntialias=*/true, Width);
	}

	FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, Geometry, MoveTemp(Points),
		ESlateDrawEffect::None, WithAlpha(Color, Opacity), /*bAntialias=*/true, Thickness);

	if (DotRadius > 0.0f)
	{
		const FVector2f DotSize(DotRadius * 2.0f, DotRadius * 2.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(Center - DotSize * 0.5f)),
			&DiscBrush(), ESlateDrawEffect::None, WithAlpha(Color, Opacity));
	}
}

int32 UPaintCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (Presence <= UE_KINDA_SMALL_NUMBER)
	{
		return Layer;
	}

	const FVector2f Center = AllottedGeometry.GetLocalSize() * 0.5f;
	Layer = PaintReticle(AllottedGeometry, OutDrawElements, Layer + 1, Center, Presence);

	// 예상 탄착 마커: 주황 바깥 링 + 흰 안쪽 링 + 흰 점. 모양과 무관하게 공통.
	const float MarkerAlpha = MarkerBlend * Presence;
	if (MarkerAlpha > UE_KINDA_SMALL_NUMBER)
	{
		DrawRing(OutDrawElements, Layer + 1, AllottedGeometry, MarkerLocal, MarkerRadius, MarkerThickness, 0.0f, MarkerColor, MarkerAlpha, 1, MarkerGlowStrength);
		DrawRing(OutDrawElements, Layer + 3, AllottedGeometry, MarkerLocal, MarkerInnerRadius, MarkerInnerThickness, MarkerDotRadius, FLinearColor::White, MarkerAlpha, 0, 0.0f);
		Layer += 4;
	}
	return Layer;
}
