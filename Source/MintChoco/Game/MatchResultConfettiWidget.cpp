#include "Game/MatchResultConfettiWidget.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

void UMatchResultConfettiWidget::SetSticker(UTexture2D* InTexture, const FMatchResultSpriteSheet& InSheet,
	const FMatchResultConfettiRules& InRules, float InStickerSize)
{
	StickerTexture = InTexture;
	Sheet = InSheet;
	Rules = InRules;
	StickerSize = FMath::Max(InStickerSize, 1.0f);

	StickerBrush.SetResourceObject(StickerTexture);
	StickerBrush.DrawAs = ESlateBrushDrawType::Image;
	StickerBrush.Tiling = ESlateBrushTileType::NoTile;
	StickerBrush.ImageSize = FVector2f(StickerSize, StickerSize);

	Clear();
}

void UMatchResultConfettiWidget::Burst(int32 Seed)
{
	if (!StickerTexture)
	{
		return;
	}
	Confetti.Start(Rules, Sheet.GetCellCount(), Seed);
	bRunning = true;
}

void UMatchResultConfettiWidget::Clear()
{
	Confetti.Reset();
	bRunning = false;
}

void UMatchResultConfettiWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMatchResultConfettiWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bRunning)
	{
		return;
	}

	Confetti.Advance(InDeltaTime);
	if (Confetti.IsDone())
	{
		// 마지막 한 장까지 사라졌다. 남은 연출 동안 빈 배열을 돌 이유가 없다.
		bRunning = false;
	}
}

int32 UMatchResultConfettiWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	const TArray<FMatchResultSticker>& Stickers = Confetti.GetStickers();
	if (!StickerTexture || Stickers.IsEmpty())
	{
		return Layer;
	}

	const FVector2f Screen = FVector2f(AllottedGeometry.GetLocalSize());
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	++Layer;
	for (const FMatchResultSticker& Sticker : Stickers)
	{
		const FVector2f Extent(StickerSize * Sticker.Scale);

		// 스티커의 자리는 가운데다. 그리는 사각형은 왼쪽 위 모서리로 잡으므로 반 장만큼 물린다.
		const FVector2f Corner = Sticker.Position * Screen - Extent * 0.5f;

		StickerBrush.SetUVRegion(Sheet.GetCellUV(Sticker.Cell));

		// 회전 중심을 넘기지 않으면 사각형의 가운데를 쓴다. 투명도가 0 이 된 장은 슬레이트가 걸러 낸다.
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Corner)), &StickerBrush,
			ESlateDrawEffect::None, FMath::DegreesToRadians(Sticker.Angle), TOptional<FVector2f>(),
			FSlateDrawElement::RelativeToElement,
			Tint.CopyWithNewOpacity(Tint.A * Rules.GetOpacity(Sticker)));
	}
	return Layer;
}
