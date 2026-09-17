#include "Game/MatchResultFrameWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UMatchResultFrameWidget::RebuildWidget()
{
	// 자식 위젯이 하나뿐이라 위젯 블루프린트를 두지 않는다. UPaintBarWidget 이 SizeBox 를 만드는 것과 같은 방식.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		FrameImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FrameImage"));
		WidgetTree->RootWidget = FrameImage;
	}
	// 이미지가 이제야 생겼다. 먼저 받아 둔 그림을 여기서 붙인다.
	ApplyFrameTexture();
	return Super::RebuildWidget();
}

void UMatchResultFrameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);
}

void UMatchResultFrameWidget::SetFrameTexture(UTexture2D* InTexture)
{
	FadeRemaining = 0.0f;
	SetRenderOpacity(0.0f);

	FrameTexture = InTexture;
	ApplyFrameTexture();
}

void UMatchResultFrameWidget::ApplyFrameTexture()
{
	if (!FrameImage)
	{
		return;
	}
	FrameImage->SetBrushFromTexture(FrameTexture, /*bMatchSize=*/false);
	// 그림이 없으면 접는다. 그대로 두면 텍스처 없는 기본 브러시가 흰 사각형으로 화면을 덮는다.
	FrameImage->SetVisibility(FrameTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UMatchResultFrameWidget::FadeIn(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		FadeRemaining = 0.0f;
		SetRenderOpacity(1.0f);
		return;
	}
	// 이미 밝아지는 중이면 남은 시간만 새로 잡는다. 지금 투명도에서 이어지므로 튀지 않는다.
	FadeRemaining = Seconds * (1.0f - GetRenderOpacity());
}

void UMatchResultFrameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FadeRemaining <= 0.0f)
	{
		return;
	}

	const float DeltaTime = FMath::Max(InDeltaTime, 0.0f);
	if (DeltaTime >= FadeRemaining)
	{
		FadeRemaining = 0.0f;
		SetRenderOpacity(1.0f);
		return;
	}

	// 남은 시간에 대한 비율로 좁힌다. 프레임이 길어도 정해진 시간에 정확히 1 이 된다.
	const float Opacity = GetRenderOpacity();
	SetRenderOpacity(Opacity + (1.0f - Opacity) * (DeltaTime / FadeRemaining));
	FadeRemaining -= DeltaTime;
}
