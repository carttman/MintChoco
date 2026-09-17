#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

#include "Game/MatchResultConfetti.h"

#include "MatchResultConfettiWidget.generated.h"

class UTexture2D;

/**
 * 승리 순간에 화면 위에서 쏟아지는 과자 스티커. 한 장짜리 스프라이트 시트에서 칸을 골라 뿌린다.
 *
 * 자식 위젯 없이 NativePaint 로 그린다(UPaintBarWidget 과 같은 방식). 위젯 트리를 만들지 않아도
 * 뷰포트에 얹힌 위젯은 화면 전체를 차지하는 자리를 받으므로, 그리는 데 필요한 크기는 NativePaint 가
 * 받는 기하에서 그대로 나온다.
 *
 * 떨어지는 계산은 FMatchResultConfetti 가 하고 여기서는 시간만 밀어 준다. 뿌리는 규칙을 바꿀 때
 * 월드를 띄우지 않고 값만 시험할 수 있어야 해서 나눠 두었다.
 */
UCLASS()
class MINTCHOCO_API UMatchResultConfettiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 그림과 격자, 뿌리는 규칙을 정한다. 뷰포트에 얹기 전에 한 번 부른다.
	 *
	 * UMatchResultFrameWidget 과 달리 위젯 트리를 쓰지 않으므로 언제 불러도 조용히 버려지지 않는다.
	 */
	void SetSticker(UTexture2D* InTexture, const FMatchResultSpriteSheet& InSheet,
		const FMatchResultConfettiRules& InRules, float InStickerSize);

	/** 뿌리기 시작한다. 다시 부르면 떠 있던 것을 지우고 처음부터. */
	void Burst(int32 Seed);

	/** 떠 있는 것까지 전부 지운다. */
	void Clear();

	/** 뿌릴 것도 없고 떠 있는 것도 없다. */
	bool IsIdle() const { return !bRunning; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> StickerTexture;

private:
	FMatchResultSpriteSheet Sheet;
	FMatchResultConfettiRules Rules;
	FMatchResultConfetti Confetti;

	/** 배율 1 인 스티커 한 장의 한 변(슬레이트 단위). */
	float StickerSize = 108.0f;

	/** 그릴 때마다 UV 만 갈아 끼우는 붓. 그리는 순간 값이 복사되므로 한 장을 돌려 써도 된다. */
	mutable FSlateBrush StickerBrush;

	bool bRunning = false;
};
