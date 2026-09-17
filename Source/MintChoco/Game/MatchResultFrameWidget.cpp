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
	bPopping = false;
	PopElapsed = 0.0f;
	SetRenderOpacity(0.0f);
	SetRenderScale(FVector2D::UnitVector);

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

void UMatchResultFrameWidget::FadeIn(const FMatchResultPop& InPop)
{
	if (bPopping)
	{
		return;
	}

	Pop = InPop;
	PopElapsed = 0.0f;
	bPopping = true;
	ApplyPop();
}

void UMatchResultFrameWidget::ApplyPop()
{
	SetRenderOpacity(Pop.GetOpacity(PopElapsed));

	// 렌더 배율은 기준점이 위젯 가운데라, 화면을 덮은 테두리가 사방으로 고르게 밀렸다 돌아온다.
	const float Scale = Pop.GetScale(PopElapsed);
	SetRenderScale(FVector2D(Scale, Scale));
}

void UMatchResultFrameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bPopping)
	{
		return;
	}

	PopElapsed += FMath::Max(InDeltaTime, 0.0f);
	ApplyPop();

	if (Pop.IsDone(PopElapsed))
	{
		// 곡선의 끝은 정확히 불투명 + 제 크기다. 남은 프레임을 계속 돌 이유가 없다.
		bPopping = false;
	}
}
