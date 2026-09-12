#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"

#include "KnockoutGaugeWidget.generated.h"

class AGameGameState;

/**
 * KO 카운트다운 게이지. 한 팀이 기준 점유율을 지키는 동안 도넛 모양 링이 돌고, 그 안쪽에
 * 남은 초가 5 · 4 · 3 · 2 · 1 로 줄어든다. 다 돌면 그 팀이 이긴다.
 *
 * 링과 숫자를 전부 NativePaint 에서 그린다(UPaintChargeWidget · UPaintCoverageBarWidget 과
 * 같은 방식). 자식 위젯이 하나도 필요 없다는 뜻이라, 어떤 HUD 에 얹든 준비할 것이 없다.
 *
 * **일부러 UGameHudWidget 과 떼어 두었다.** HUD 가 나중에 다른 사람이 만든 것으로 통째로
 * 바뀔 수 있으므로, 이 위젯은 부모를 전혀 모른다: 자기가 직접 AGameGameState 를 찾고
 * 자기 표시 여부까지 스스로 정한다.
 *
 * 붙이는 방법 셋 중 아무거나:
 *
 * 1. 이 클래스(또는 WBP_KnockoutGauge)를 HUD 어딘가에 놓기만 한다. 설정할 것이 없다.
 * 2. 위젯 BP 로 상속해 색·굵기·폰트만 바꾸고, 연출은 BP_OnKnockoutChanged 로 얹는다.
 * 3. 이 위젯을 아예 쓰지 않고, 자기 위젯에서 AGameGameState 의 Knockout* 함수들을 읽는다.
 *    그쪽이 원본이고 이 위젯은 그것을 읽는 한 가지 예일 뿐이다.
 *
 * 70 %까지 칠하지 않고 모양만 보려면 콘솔에서 `mc.KnockoutPreview 0.4`.
 */
UCLASS()
class MINTCHOCO_API UKnockoutGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UKnockoutGaugeWidget(const FObjectInitializer& ObjectInitializer);

	/** 이 위젯이 읽는 GameState. 게임 맵이 아니면 null 이고, 그때는 게이지가 영영 숨어 있다. */
	UFUNCTION(BlueprintPure, Category = "Knockout")
	AGameGameState* GetGameState() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** 도넛의 반지름(px). 안쪽 구멍의 크기는 이 값에서 Thickness 의 절반을 뺀 만큼이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout", meta = (ClampMin = "4", ForceUnits = "px"))
	float Radius = 26.0f;

	/** 도넛 테두리의 굵기(px). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float Thickness = 6.0f;

	/** 안쪽 숫자의 폰트. 기본은 굵은 20 pt 에 검은 외곽선. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout")
	FSlateFontInfo Font;

	/**
	 * 링을 그 팀 색으로 물들일지. 끄면 Color 가 그대로 쓰인다.
	 * 색은 Teams::GetDisplayColor 한 곳에서 오므로 점유율 게이지·로비와 어긋나지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout")
	bool bTintByTeam = true;

	/** 팀 색을 쓰지 않을 때의 색, 그리고 숫자의 색. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout")
	FLinearColor Color = FLinearColor(1.0f, 0.25f, 0.25f);

	/** 12시에서 시작해 시계 방향으로 돈다. 끄면 반시계. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout")
	bool bClockwise = true;

	/**
	 * 참이면 꽉 찬 링이 시간이 갈수록 줄어든다(모래시계). 거짓이면 빈 링이 차오른다(KO 까지의 진행).
	 * 기본은 차오르는 쪽 — 링이 닫히는 순간이 곧 KO 라 위협이 눈에 더 잘 들어온다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout")
	bool bDrain = false;

	/** 채워지지 않은 나머지. 이것이 있어야 짧은 호가 "이제 막 시작한 게이지"로 읽힌다. 0 이면 숨는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout|Track", meta = (ClampMin = "0", ClampMax = "1"))
	float TrackOpacity = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout|Track", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float TrackThickness = 6.0f;

	/** 도넛 안쪽을 덮는 판. 숫자가 배경 위에서도 읽히도록. 알파 0 이면 그리지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout|Track")
	FLinearColor HoleColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.65f);

	/** 링 아래에 깔리는 후광. 넓고 흐린 같은 호를 겹쳐 그린다. 0 이면 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout|Glow", meta = (ClampMin = "0", ClampMax = "4"))
	int32 GlowLayers = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout|Glow", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.3f;

	/** 남은 1초 동안 숫자가 이 배율까지 커졌다 돌아온다. 1 이면 크기가 변하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout", meta = (ClampMin = "1", ClampMax = "3", ForceUnits = "x"))
	float TickPulse = 1.25f;

	/** 한 바퀴를 이루는 선분 수. 호는 그 비율만큼만 쓰므로 길이와 무관하게 밀도가 같다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knockout", meta = (ClampMin = "8"))
	int32 Segments = 64;

	/**
	 * 카운트다운이 시작되거나 끝날 때 한 번씩. 효과음이나 애니메이션을 붙이는 자리.
	 * bPending 이 거짓이면 Team 은 직전까지 노리던 팀이다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Knockout")
	void BP_OnKnockoutChanged(bool bPending, int32 Team);

	/** 안쪽 숫자가 바뀔 때마다(5 → 4 → ...). 초 단위 효과음을 붙이는 자리. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Knockout")
	void BP_OnKnockoutSecond(int32 SecondsLeft, int32 Team);

private:
	/** 링에 그릴 점들. Fraction 1 이면 원이 닫힌다. */
	void BuildArc(const FVector2f& Center, float Fraction, TArray<FVector2f>& OutPoints) const;

	/** 표시 여부와 BP 알림을 한 곳에서 바꾼다. */
	void SetPending(bool bPending, int32 Team);

	/** 지금 링에 그릴 색. */
	FLinearColor GetRingColor() const;

	bool bPendingNow = false;
	int32 PendingTeam = INDEX_NONE;

	/** NativePaint 는 const 라 틱에서 재 둔다. */
	float Progress = 0.0f;
	float Remaining = 0.0f;

	/** 화면에 보이는 숫자. 바뀌는 순간을 잡아 BP_OnKnockoutSecond 를 부른다. */
	int32 ShownSecond = 0;

	/** 숫자가 바뀐 뒤 지난 초. 커졌다 돌아오는 연출의 기준. */
	float SinceSecondChanged = 0.0f;
};
