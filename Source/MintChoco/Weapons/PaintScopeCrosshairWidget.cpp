#include "Weapons/PaintScopeCrosshairWidget.h"

#include "Rendering/DrawElements.h"

#include "Weapons/PaintWeaponComponent.h"

void UPaintScopeCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 미리보기 1(사격)은 가득 찬 충전으로 본다: 홀드 중 모양과 풀차지 색을 한 번에 확인한다.
	const int32 Preview = GetPreviewState();
	const UPaintWeaponComponent* const Weapon = GetWeapon();
	const bool bCharged = Preview >= 0 ? Preview == 1 : (Weapon && Weapon->GetChargeFraction() >= 1.0f);
	ChargedBlend = FMath::FInterpTo(ChargedBlend, bCharged ? 1.0f : 0.0f, InDeltaTime, ChargedBlendSpeed);
}

int32 UPaintScopeCrosshairWidget::PaintReticle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2f& Center, float Alpha) const
{
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	const float Scale = FMath::Lerp(1.0f, FiringScale, GetFiringBlend());
	const FLinearColor Blended = FMath::Lerp(ScopeColor, ChargedColor, ChargedBlend);
	const FLinearColor Color(Blended.R, Blended.G, Blended.B, Blended.A * Alpha);

	// 눈금 넷: 안쪽 반지름에서 원까지, 12·3·6·9시.
	const float Inner = TickInnerRadius * Scale;
	const float Outer = ScopeRadius * Scale;
	for (const FVector2f& Axis : { FVector2f(0.0f, -1.0f), FVector2f(1.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(-1.0f, 0.0f) })
	{
		TArray<FVector2f> Points;
		Points.Add(Center + Axis * Inner);
		Points.Add(Center + Axis * Outer);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry, MoveTemp(Points),
			ESlateDrawEffect::None, Color, /*bAntialias=*/true, TickThickness);
	}

	// 바깥 원 + 중앙 점. 풀차지에는 얇은 글로우 한 겹.
	DrawRing(OutDrawElements, Layer + 1, AllottedGeometry, Center, Outer, ScopeThickness, DotRadius * Scale, Color, 1.0f,
		ChargedBlend > UE_KINDA_SMALL_NUMBER ? 1 : 0, 0.5f * ChargedBlend);
	return Layer + 2;
}
