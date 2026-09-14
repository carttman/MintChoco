#include "Weapons/PaintBracketCrosshairWidget.h"

#include "Rendering/DrawElements.h"

int32 UPaintBracketCrosshairWidget::PaintReticle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2f& Center, float Alpha) const
{
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	const FVector2f HalfExtent(BracketHalfExtent);

	// 브래킷: 꼭짓점에서 안쪽으로 꺾인 팔 둘. 상태와 무관하게 고정.
	const FLinearColor Bracket(BracketColor.R, BracketColor.G, BracketColor.B, BracketColor.A * Alpha);
	for (const FVector2f& Sign : { FVector2f(-1.0f, -1.0f), FVector2f(1.0f, -1.0f), FVector2f(-1.0f, 1.0f), FVector2f(1.0f, 1.0f) })
	{
		const FVector2f Corner = Center + Sign * HalfExtent;
		TArray<FVector2f> Points;
		Points.Add(Corner + FVector2f(0.0f, -Sign.Y * BracketArm));
		Points.Add(Corner);
		Points.Add(Corner + FVector2f(-Sign.X * BracketArm, 0.0f));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry, MoveTemp(Points),
			ESlateDrawEffect::None, Bracket, /*bAntialias=*/true, BracketThickness);
	}

	// 중앙 원: 기본↔사격을 블렌드하고, 마커가 보이는 만큼 눌러 반투명하게.
	const float Firing = GetFiringBlend();
	const float Radius = FPaintCrosshairMath::CenterRadius(CenterRadius, FiringScale, Firing);
	const float Opacity = FPaintCrosshairMath::CenterOpacity(IdleOpacity, FiringOpacity, Firing, GetMarkerBlend(), CenterDimmedOpacity) * Alpha;
	const FLinearColor CenterColor = FMath::Lerp(IdleColor, FiringColor, Firing);
	const float DotRadius = CenterDotRadius * FMath::Lerp(1.0f, FiringScale, Firing);
	DrawRing(OutDrawElements, Layer + 1, AllottedGeometry, Center, Radius, CenterThickness, DotRadius, CenterColor, Opacity,
		Firing > UE_KINDA_SMALL_NUMBER ? FiringGlowLayers : 0, GlowStrength * Firing);
	return Layer + 2;
}
