#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "MatchResultFrameWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 결과 화면을 두르는 장식 테두리. 화면 전체를 덮는 이미지 한 장이 전부다.
 *
 * 연출이 시작될 때 완전히 투명한 채로 얹혔다가, 이긴 쪽이 승리 모션에 들어가는 순간 같이 밝아진다.
 * 페이드는 자기 틱에서 돌린다: 감독(UMatchResultSubsystem)은 단계마다 타이머만 걸고 매 프레임을
 * 돌지 않으므로, 켜고 끄는 신호만 받고 나머지는 위젯이 알아서 하는 편이 배선이 적다.
 *
 * 그림이 위쪽과 양옆에만 있고 가운데와 아래는 비어 있어서, 화면 아래의 점유율 바와 겹치지 않는다.
 * 그래도 바보다 아래 ZOrder 로 얹어 바가 가려질 일이 없게 한다.
 */
UCLASS()
class MINTCHOCO_API UMatchResultFrameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 테두리 그림을 정하고 완전히 투명한 상태로 되돌린다. 비어 있으면 아무것도 그리지 않는다. */
	void SetFrameTexture(UTexture2D* InTexture);

	/** 지금 투명도에서 Seconds 동안 불투명해진다. 0 이하면 즉시 불투명해진다. */
	void FadeIn(float Seconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FrameImage;

	/**
	 * 그릴 그림. 위젯을 만든 직후에는 트리가 아직 없으므로(RebuildWidget 은 뷰포트에 얹힐 때 돈다)
	 * 여기 담아 두었다가 이미지가 생긴 뒤에 붙인다. 바로 붙이려 들면 조용히 버려지고, 텍스처 없는
	 * 기본 브러시가 남아 화면 전체가 흰색으로 덮인다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FrameTexture;

private:
	/** 담아 둔 그림을 이미지에 붙인다. 이미지가 아직 없으면 아무것도 하지 않는다. */
	void ApplyFrameTexture();

	/** 남은 페이드 시간(초). 0 이면 페이드가 끝났거나 시작하지 않았다. */
	float FadeRemaining = 0.0f;
};
