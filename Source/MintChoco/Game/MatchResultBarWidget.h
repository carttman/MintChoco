#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

#include "Game/PaintBar.h"
#include "Game/PaintBarClashEffect.h"
#include "Game/TeamTypes.h"

#include "MatchResultBarWidget.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USizeBox;

/** 액체 안에 겹치는 가로 물결 한 겹. 물결선 아래쪽이 Shade만큼 밝거나 어두워진다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FMatchResultBarWave
{
	GENERATED_BODY()

	/** 물결 기준선의 높이. 액체 위(0)부터 아래(1)까지의 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float Height = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Bar", meta = (ClampMin = "0"))
	float Amplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Bar", meta = (ClampMin = "1"))
	float Length = 160.0f;

	/** 가운데 쪽으로 흘러가는 속도(단위/초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Bar")
	float Speed = 24.0f;

	/** 물결선 아래쪽의 명암. 양수면 흰색 쪽으로 밝게, 음수면 어둡게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Bar", meta = (ClampMin = "-1", ClampMax = "1"))
	float Shade = 0.0f;
};

/**
 * 끝난 경기의 점유율을 보여 주는 결과 화면의 바. UMatchResultSubsystem 이 단계마다 값을 밀어 넣고
 * (빈 바 → 맛보기 → 진짜 비율 → 마무리), 마지막에 이긴 팀을 들려 보내면 진 쪽 액체가 탁해지고 이긴 쪽
 * 게이지가 실제보다 더 밀려 들어간다.
 *
 * 경기 중 HUD 의 UPaintBarWidget 과 지금은 같은 그림이지만 같은 코드가 아니다. 이쪽은 재생 화면이라
 * 셀 것도 위험도 없다 - 카운트다운 링과 위험 점멸이 없고, 판정선과 "KO" 글자만 자리를 지킨다.
 * 마음껏 다른 모양으로 가도 경기 화면은 따라 흔들리지 않는다.
 *
 * 액체는 BarMaterial(M_UI_PaintBar) 한 장으로 그리고, 판정선·글자·격돌 연출은 NativePaint에서 그린다.
 * 게이지 길이는 FPaintBarMath 로 재므로 경기와 같은 자다 - 격돌 문턱과 판정선은 SetMatchRules 로 받는다.
 */
UCLASS()
class MINTCHOCO_API UMatchResultBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMatchResultBarWidget(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;

	/** 이번 단계에 보여 줄 비율과, 마무리를 걸 이긴 팀(Teams::None 이면 걸지 않는다). */
	void SetShownCoverage(float InLeftCoverage, float InRightCoverage, int32 InKnockoutWinner);

	/** 경기와 같은 자로 재도록 격돌 문턱과 판정선을 받는다. 이 바는 AGameGameState 를 읽지 않는다. */
	void SetMatchRules(float InClashCoverage, float InKoLine);

	FVector2D GetBarSize() const { return BarSize; }

	/** 바의 크기를 바꾼다. 결과 화면은 경기 중 HUD 보다 큰 바를 쓴다. */
	void SetBarSize(const FVector2D& InBarSize);

	/** 값이 바뀔 때 게이지가 따라가는 시간. 느리게 두면 차오르는 것이 보인다. */
	void SetFillSmoothingSeconds(float InSeconds) { FillSmoothingSeconds = FMath::Max(InSeconds, 0.0f); }

	int32 GetLeftPaintId() const { return LeftPaintId; }
	int32 GetRightPaintId() const { return RightPaintId; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	//~ 자리

	/** 왼쪽 끝에서 차오르는 팀의 페인트 id. 결과 화면의 캐릭터도 이 값을 따라 좌우가 정해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Teams", meta = (ClampMin = "0", ClampMax = "6"))
	int32 LeftPaintId = Teams::Mint;

	/** 오른쪽 끝에서 차오르는 팀의 페인트 id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Teams", meta = (ClampMin = "0", ClampMax = "6"))
	int32 RightPaintId = Teams::Choco;

	//~ 모양

	/** M_UI_PaintBar나 그 인스턴스. 비어 있으면 액체를 그리지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Look")
	TObjectPtr<UMaterialInterface> BarMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Look")
	FVector2D BarSize = FVector2D(1080.0, 44.0);

	/** 흰 통의 테두리와 액체 사이 간격. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Look", meta = (ClampMin = "0", ForceUnits = "px"))
	float ShellPadding = 3.0f;

	/** 액체 색은 LeftPaintId/RightPaintId 의 팀 색(TeamLook, MPC_TeamLook)이다. 여기서 따로 정하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Look")
	FLinearColor ShellColor = FLinearColor(FColor(252, 252, 254));

	/** 값이 바뀔 때 게이지가 따라가는 시간. 단계가 바뀌는 순간마다 이 시간 동안 차오른다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Look", meta = (ClampMin = "0", ForceUnits = "s"))
	float FillSmoothingSeconds = 0.45f;

	/** 위에서 아래로 겹치는 물결 세 겹. 뒤의 물결이 앞의 물결 아래쪽을 덮는다. */
	UPROPERTY(EditAnywhere, Category = "Result Bar|Waves")
	FMatchResultBarWave Waves[3];

	/** 가장 뒤 물결(Waves[0])선 위쪽의 불투명도. 0이면 흰 통이 비쳐 액체 윗면이 물결 모양으로 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float TopLayerOpacity = 0.0f;

	/** 액체 앞머리(세로 경계)가 흔들리는 폭. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Waves", meta = (ClampMin = "0"))
	float FrontAmplitude = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Waves", meta = (ClampMin = "1"))
	float FrontLength = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Waves")
	float FrontSpeed = 2.2f;

	//~ 판정선

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark")
	FText KoText = NSLOCTEXT("MatchResultBar", "KoLabel", "KO");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark")
	FSlateFontInfo KoFont;

	/**
	 * 판정선과 그 글자의 색은 그 선을 넘어야 하는 팀(반대쪽 팀)의 색을 어둡게 쓴다.
	 * 팀 색의 HSV 명도에 곱하는 배율이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark", meta = (ClampMin = "0", ClampMax = "1"))
	float MarkDarken = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float MarkThickness = 3.0f;

	/** 판정선이 바 위로 튀어나오는 길이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark", meta = (ClampMin = "0", ForceUnits = "px"))
	float MarkOverhangTop = 6.0f;

	/** 판정선이 바 아래로 튀어나오는 길이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark", meta = (ClampMin = "0", ForceUnits = "px"))
	float MarkOverhangBottom = 10.0f;

	/** "KO" 글자 아래쪽과 판정선 위끝 사이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Mark", meta = (ForceUnits = "px"))
	float LabelGap = 0.0f;

	//~ 마무리

	/** 진 팀 색이 탁해지는 정도. 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Finish", meta = (ClampMin = "0", ClampMax = "1"))
	float LoserDullAmount = 0.45f;

	/** 이긴 팀 게이지를 실제보다 이만큼 더 밀어 넣는다. 게이지 길이에 대한 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Finish", meta = (ClampMin = "0", ClampMax = "0.5"))
	float WinnerExaggeration = 0.08f;

	/** 마무리가 올라오는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Finish", meta = (ClampMin = "0", ForceUnits = "s"))
	float FinishBlendSeconds = 0.6f;

	//~ 격돌

	/** 두 액체가 만났을 때 도는 연출. 종류를 골라 담고, 여러 개를 겹칠 수 있다. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Result Bar|Clash")
	TArray<TObjectPtr<UPaintBarClashEffect>> ClashEffects;

	/** 두 액체가 만났을 때 연출이 올라오는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Clash", meta = (ClampMin = "0", ForceUnits = "s"))
	float ClashFadeInSeconds = 0.08f;

	/** 떨어지거나 마무리가 걸렸을 때 연출이 가라앉는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Clash", meta = (ClampMin = "0", ForceUnits = "s"))
	float ClashFadeOutSeconds = 0.35f;

	//~ 규칙

	/** 두 팀이 칠한 합이 이 비율에 닿으면 두 게이지가 가운데에서 만난다. SetMatchRules 가 경기 값으로 덮는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Rules", meta = (ClampMin = "0.01", ClampMax = "1"))
	float ClashCoverage = 0.6f;

	/** 양 끝에서 판정선까지의 거리. 0 이면 선과 글자를 그리지 않는다. SetMatchRules 가 경기 값으로 덮는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Bar|Rules", meta = (ClampMin = "0", ClampMax = "0.45"))
	float KoLine = 0.3f;

	//~ 미리보기

	/** 위젯 디자이너에서만 쓰는 값. 게임에서는 밀어 넣은 값이 이긴다. */
	UPROPERTY(EditAnywhere, Category = "Result Bar|Preview")
	bool bDesignerPreview = true;

	UPROPERTY(EditAnywhere, Category = "Result Bar|Preview", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bDesignerPreview"))
	float DesignerLeftCoverage = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Result Bar|Preview", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bDesignerPreview"))
	float DesignerRightCoverage = 0.3f;

	/** 미리보기에서 마무리를 걸 팀. Teams::None 이면 걸지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Result Bar|Preview", meta = (ClampMin = "-1", ClampMax = "6", EditCondition = "bDesignerPreview"))
	int32 DesignerKnockoutWinner = Teams::None;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootBox;

private:
	void UpdateFinish(float DeltaTime);
	void UpdateClash(float DeltaTime);
	void UpdateMaterial();
	bool EnsureMaterialInstance();

	/** 이 팀이 졌는지. 이긴 팀이 반대편이면 진 것이다. */
	bool IsLoser(bool bLeft) const;

	/** 마무리가 얼마나 올라왔는지. 진 쪽만 0 보다 크다. */
	float GetFinishBlend(bool bLeft) const { return bLeft ? LeftFinishBlend : RightFinishBlend; }

	FLinearColor GetTeamColor(bool bLeft) const;
	FLinearColor GetDulledColor(bool bLeft) const;
	FLinearColor GetMarkColor(bool bLeft) const;
	float GetInnerSpan(const FVector2f& Size) const;
	float GetMarkX(const FVector2f& Size, bool bLeft) const;
	FPaintBarClashFrame MakeClashFrame() const;

	int32 PaintMark(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const;
	int32 PaintLabel(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarMaterialInstance;

	FSlateBrush BarBrush;

	/** 밀어 넣은 값. 디자이너에서는 DesignerPreview* 가 대신 온다. */
	FVector2f ShownCoverage = FVector2f::ZeroVector;
	int32 KnockoutWinner = Teams::None;

	/** 따라가는 중인 커버리지. X가 왼쪽 팀, Y가 오른쪽 팀. */
	FVector2f SmoothedCoverage = FVector2f::ZeroVector;
	bool bHasCoverage = false;

	/** 과장까지 적용해 화면에 그리는 게이지. */
	FPaintBarFill DisplayedFill;

	float LeftFinishBlend = 0.0f;
	float RightFinishBlend = 0.0f;

	FVector2f LocalSize = FVector2f::ZeroVector;
	float WaveTime = 0.0f;
	float ClashStrength = 0.0f;
	bool bWasClashing = false;
	float LastContactX = 0.0f;
	float ContactVelocity = 0.0f;
};
