#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

#include "Game/PaintBar.h"
#include "Game/PaintBarClashEffect.h"
#include "Game/TeamTypes.h"

#include "PaintBarWidget.generated.h"

class AGameGameState;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USizeBox;
struct FPaintCoverage;

/** 액체 안에 겹치는 가로 물결 한 겹. 물결선 아래쪽이 Shade만큼 밝거나 어두워진다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintBarWave
{
	GENERATED_BODY()

	/** 물결 기준선의 높이. 액체 위(0)부터 아래(1)까지의 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float Height = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "0"))
	float Amplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "1"))
	float Length = 160.0f;

	/** 가운데 쪽으로 흘러가는 속도(단위/초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar")
	float Speed = 24.0f;

	/** 물결선 아래쪽의 명암. 양수면 흰색 쪽으로 밝게, 음수면 어둡게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint Bar", meta = (ClampMin = "-1", ClampMax = "1"))
	float Shade = 0.0f;
};

/**
 * 두 팀이 칠한 비율을 흰 통 안의 액체로 보여주는 바. 왼쪽 팀은 왼쪽 끝, 오른쪽 팀은 오른쪽 끝에서 차오르고,
 * 둘이 만나면 ClashEffects가 돈다. 판정선은 양 끝에서 KoLine 만큼 들어온 고정 자리다. 게이지 길이와 선을
 * AGameGameState 와 같은 값(ClashCoverage, KnockoutLine)으로 재므로 상대 게이지가 선에 닿는 순간이 곧
 * 서버가 세기 시작하는 순간이다. 상대가 선에 다가오면 빨갛게 점멸하고, 링은 GameState 가 복제한 카운트다운을
 * 그대로 보여 준다.
 *
 * 액체는 BarMaterial(M_UI_PaintBar) 한 장으로 그리고, 판정선·글자·링·격돌 연출은 NativePaint에서 그린다.
 * 트리가 비어 있으면 BarSize 크기의 SizeBox를 루트로 만들어 그대로 배치할 수 있다. GameState 가 없는 곳
 * (샘플 맵, 디자이너 미리보기)에서만 Rules 의 값으로 로컬 시계를 돌린다.
 */
UCLASS()
class MINTCHOCO_API UPaintBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPaintBarWidget(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;

	/** 실제 커버리지 대신 쓸 값. bEnabled가 false면 다시 월드 커버리지를 읽는다. 디자이너 미리보기와는 따로 논다. */
	UFUNCTION(BlueprintCallable, Category = "Paint Bar")
	void SetCoverageOverride(const FPaintBarPreview& InOverride);

	FVector2D GetBarSize() const { return BarSize; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	//~ 규칙

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Rules")
	FPaintBarRules Rules;

	/** 왼쪽 끝에서 차오르는 팀의 페인트 id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Rules", meta = (ClampMin = "0", ClampMax = "6"))
	int32 LeftPaintId = Teams::Mint;

	/** 오른쪽 끝에서 차오르는 팀의 페인트 id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Rules", meta = (ClampMin = "0", ClampMax = "6"))
	int32 RightPaintId = Teams::Choco;

	//~ 모양

	/** M_UI_PaintBar나 그 인스턴스. 비어 있으면 액체를 그리지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Look")
	TObjectPtr<UMaterialInterface> BarMaterial;

	/** 바의 크기. 판정선 글자와 링은 이 바깥에 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Look")
	FVector2D BarSize = FVector2D(860.0, 30.0);

	/** 흰 통의 테두리와 액체 사이 간격. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Look", meta = (ClampMin = "0", ForceUnits = "px"))
	float ShellPadding = 3.0f;

	/** 액체 색은 LeftPaintId/RightPaintId 의 팀 색(TeamLook, MPC_TeamLook)이다. 여기서 따로 정하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Look")
	FLinearColor ShellColor = FLinearColor(FColor(252, 252, 254));

	/** 값이 바뀔 때 게이지가 따라가는 시간. 커버리지는 0.2초마다 복제되어 그대로 쓰면 계단처럼 튄다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Look", meta = (ClampMin = "0", ForceUnits = "s"))
	float FillSmoothingSeconds = 0.25f;

	/** 위에서 아래로 겹치는 물결 세 겹. 뒤의 물결이 앞의 물결 아래쪽을 덮는다. */
	UPROPERTY(EditAnywhere, Category = "Paint Bar|Waves")
	FPaintBarWave Waves[3];

	/** 가장 뒤 물결(Waves[0])선 위쪽의 불투명도. 0이면 흰 통이 비쳐 액체 윗면이 물결 모양으로 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float TopLayerOpacity = 0.0f;

	/** 액체 앞머리(세로 경계)가 흔들리는 폭. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Waves", meta = (ClampMin = "0"))
	float FrontAmplitude = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Waves", meta = (ClampMin = "1"))
	float FrontLength = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Waves")
	float FrontSpeed = 2.2f;

	//~ 위험 점멸

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Danger")
	FLinearColor DangerColor = FLinearColor(FColor(255, 42, 42));

	/** 가장 빨갈 때 원래 색을 덮는 정도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Danger", meta = (ClampMin = "0", ClampMax = "1"))
	float DangerStrength = 0.85f;

	/** 빨개졌다가 원래 색으로 돌아오는 한 주기. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Danger", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float DangerPulsePeriod = 0.9f;

	/** 위험에 들어가고 나올 때 점멸이 켜지고 꺼지는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Danger", meta = (ClampMin = "0", ForceUnits = "s"))
	float DangerFadeSeconds = 0.3f;

	//~ KO

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO")
	FText KoText = NSLOCTEXT("PaintBar", "KoLabel", "KO");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO")
	FSlateFontInfo KoFont;

	/**
	 * 판정선과 그 글자·링의 색은 그 선을 넘어야 하는 팀(반대쪽 팀)의 색을 어둡게 쓴다.
	 * 팀 색의 HSV 명도에 곱하는 배율이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ClampMax = "1"))
	float MarkDarken = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float MarkThickness = 3.0f;

	/** 판정선이 바 위로 튀어나오는 길이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ForceUnits = "px"))
	float MarkOverhangTop = 6.0f;

	/** 판정선이 바 아래로 튀어나오는 길이. 링이 뜨면 링까지 내려간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ForceUnits = "px"))
	float MarkOverhangBottom = 10.0f;

	/** "KO" 글자 아래쪽과 판정선 위끝 사이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ForceUnits = "px"))
	float LabelGap = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring", meta = (ClampMin = "4", ForceUnits = "px"))
	float RingRadius = 17.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring", meta = (ClampMin = "1", ForceUnits = "px"))
	float RingThickness = 4.0f;

	/** 바 아래쪽과 링 위쪽 사이. 판정선이 이만큼 내려와 링에 닿는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring", meta = (ClampMin = "0", ForceUnits = "px"))
	float RingGap = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring")
	FSlateFontInfo RingFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring")
	FLinearColor RingBackgroundColor = FLinearColor::White;

	/** 아직 차지 않은 링 부분의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO|Ring")
	FLinearColor RingTrackColor = FLinearColor(FColor(222, 226, 224));

	/** KO된 팀 색이 탁해지는 정도. 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ClampMax = "1"))
	float KoDullAmount = 0.45f;

	/** KO가 나면 이긴 팀 게이지를 실제보다 이만큼 더 밀어 넣는다. 게이지 길이에 대한 비율이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ClampMax = "0.5"))
	float KoExaggeration = 0.08f;

	/** KO 연출이 켜지고 꺼지는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|KO", meta = (ClampMin = "0", ForceUnits = "s"))
	float KoBlendSeconds = 0.6f;

	//~ 격돌

	/** 두 액체가 만났을 때 도는 연출. 종류를 골라 담고, 여러 개를 겹칠 수 있다. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Paint Bar|Clash")
	TArray<TObjectPtr<UPaintBarClashEffect>> ClashEffects;

	/** 두 액체가 만났을 때 연출이 올라오는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Clash", meta = (ClampMin = "0", ForceUnits = "s"))
	float ClashFadeInSeconds = 0.08f;

	/** 떨어지거나 KO가 났을 때 연출이 가라앉는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Bar|Clash", meta = (ClampMin = "0", ForceUnits = "s"))
	float ClashFadeOutSeconds = 0.35f;

	//~ 미리보기

	/** 위젯 디자이너에서만 쓰는 값. 게임에서는 무시한다. */
	UPROPERTY(EditAnywhere, Category = "Paint Bar|Preview")
	FPaintBarPreview DesignerPreview;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootBox;

private:
	/** 한쪽 KO 시계가 지금 어떤지. 경기에서는 GameState 의 복제값, 밖에서는 로컬 시계에서 온다. */
	struct FKoStatus
	{
		bool bCounting = false;
		float Progress = 0.0f;
		int32 SecondsLeft = 0;
		bool bKnockedOut = false;
	};

	/** 한쪽 팀이 밀릴 때의 연출 상태. 왼쪽 상태는 오른쪽 팀이 밀어붙이는 상황이다. */
	struct FSideState
	{
		/** GameState 가 없을 때만 도는 로컬 시계. */
		FPaintKoClock Clock;
		bool bKnockedOut = false;
		float DangerEnvelope = 0.0f;
		float DangerPhase = 0.0f;
		float KoBlend = 0.0f;
		float RingAlpha = 0.0f;
		float RingAge = 0.0f;
		float RingProgress = 0.0f;
		int32 RingNumber = 0;
	};

	FVector2f ReadCoverage(float DeltaTime);
	FVector2f CoverageOf(const FPaintCoverage& Coverage) const;
	/** 미리보기 중이면 nullptr. 판정은 GameState 가 있을 때만 그쪽을 믿는다. */
	const AGameGameState* FindRuleSource() const;
	FKoStatus MakeKoStatus(FSideState& Side, const AGameGameState* GameState, int32 OpponentPaintId, float OpponentFill,
		float KoHoldSeconds, float DeltaTime) const;
	void UpdateSide(FSideState& Side, float OpponentFill, const FKoStatus& Ko, float DeltaTime) const;
	void UpdateClash(float DeltaTime);
	void UpdateMaterial();
	bool EnsureMaterialInstance();

	float GetDanger(const FSideState& Side) const;
	FLinearColor GetDisplayedColor(const FLinearColor& Base, const FSideState& Side) const;
	FLinearColor GetTeamColor(bool bLeft) const;
	FLinearColor GetMarkColor(bool bLeft) const;
	float GetInnerSpan(const FVector2f& Size) const;
	float GetMarkX(const FVector2f& Size, bool bLeft) const;
	FPaintBarClashFrame MakeClashFrame() const;

	int32 PaintMark(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft, const FSideState& Side) const;
	int32 PaintLabel(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft) const;
	int32 PaintRing(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bLeft, const FSideState& Side) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarMaterialInstance;

	FSlateBrush BarBrush;
	FPaintBarPreview CoverageOverride;
	FSideState LeftSide;
	FSideState RightSide;

	/** 따라가는 중인 커버리지. X가 왼쪽 팀, Y가 오른쪽 팀. */
	FVector2f SmoothedCoverage = FVector2f::ZeroVector;
	bool bHasCoverage = false;

	/** 과장까지 적용해 화면에 그리는 게이지. */
	FPaintBarFill DisplayedFill;

	/** 이번 틱의 판정선 거리(양 끝에서, 게이지 길이 비율). GameState 가 있으면 그 값, 없으면 Rules.KoLine. 0 이면 KO 가 꺼진 것이라 선·글자·링을 그리지 않는다. */
	float KoLine = 0.0f;

	FVector2f LocalSize = FVector2f::ZeroVector;
	float WaveTime = 0.0f;
	float DemoTime = 0.0f;
	float ClashStrength = 0.0f;
	bool bWasClashing = false;
	float LastContactX = 0.0f;
	float ContactVelocity = 0.0f;
};
