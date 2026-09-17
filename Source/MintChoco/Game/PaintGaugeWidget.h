#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

#include "Game/PaintBarWidget.h"

#include "PaintGaugeWidget.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USizeBox;

/** 게이지의 순수 계산. 월드 없이 테스트한다. */
struct MINTCHOCO_API FPaintGaugeMath
{
	/**
	 * 지수 보간 한 걸음. SmoothingSeconds는 목표까지의 차이가 대략 1/e로 줄어드는 시간이다.
	 * 0 이하면 보간 없이 곧바로 목표다.
	 *
	 * 지수 보간은 목표에 닿지 않으므로 아주 가까워지면 붙인다. 안 그러면 잉크가 가득 찼는데도
	 * 게이지가 영원히 조금 모자란 채로 남는다.
	 */
	static float Approach(float Current, float Target, float DeltaTime, float SmoothingSeconds);

	/** 게이지가 그릴 값. 잉크는 이미 0~1이지만 복제 값이라 범위를 믿지 않는다. */
	static float FillFromInk(float Ink) { return FMath::Clamp(Ink, 0.0f, 1.0f); }
};

/**
 * 내가 가진 페인트(잉크)를 초승달 안에 찬 액체로 보여 주는 게이지.
 *
 * HUD의 Img_Info(Character_Case) 위에 같은 사각형으로 겹쳐 놓는다. 초승달의 흰 부분은 그 아이콘이
 * 그리는 불투명한 픽셀이라 밑에 깔면 가려지므로, 위에 얹고 머티리얼이 초승달 안쪽만 칠한다.
 *
 * 모양은 아이콘 그림이 정한다: 머티리얼이 IconTexture를 샘플링해 "이 픽셀이 흰가"로 경계를 잡는다.
 * 아래 원 두 개는 모양이 아니라 울타리다 — 아이콘 다른 곳의 흰 점들이 액체 알갱이로 튀지 않게 막는다.
 *
 * 원 두 개로 모양까지 맞춰 보았으나 되지 않았다. 몸통은 맞는데(최선이 IoU 0.81) 아트의 초승달은 끝이
 * 뭉툭하게 잘려 있고 수학적 초승달은 뾰족한 첨점으로 끝나, 그 자리에서 액체가 테두리 위로 넘쳤다.
 *
 * 단위는 GaugeSize 기준의 픽셀이고, 위젯이 커지거나 작아져도 아이콘과 같은 비율로 늘어난다.
 *
 * 액체는 GaugeMaterial(M_UI_PaintGauge) 한 장으로 그린다. 물결은 커버리지 바와 같은 FPaintBarWave라
 * 값을 그대로 베껴 올 수 있다 — 다만 세로 게이지에서는 가장 위 물결선이 곧 액체의 윗면이 된다.
 */
UCLASS()
class MINTCHOCO_API UPaintGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPaintGaugeWidget(const FObjectInitializer& ObjectInitializer);

	/** 실제 잉크 대신 쓸 값(0~1). bEnabled가 false면 다시 내 유닛을 읽는다. */
	UFUNCTION(BlueprintCallable, Category = "Paint Gauge")
	void SetFillOverride(bool bEnabled, float Fill);

	virtual void SynchronizeProperties() override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** M_UI_PaintGauge나 그 인스턴스. 비어 있으면 아무것도 그리지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Look")
	TObjectPtr<UMaterialInterface> GaugeMaterial;

	/**
	 * 아래 초승달 좌표의 기준이 되는 크기. Img_Info(Character_Case)와 같은 214×230이다.
	 *
	 * 머티리얼은 언제나 이 크기의 좌표계에서 계산하므로, 위젯이 화면에서 커지거나 작아져도 원이
	 * 찌그러지지 않고 아이콘과 같은 비율로 따라간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Look")
	FVector2D GaugeSize = FVector2D(214.0, 230.0);

	//~ 울타리. 큰 원에서 안쪽 원을 뺀 영역이다. 전부 GaugeSize 기준의 픽셀.
	//~ 기본값은 Character_Case의 흰 초승달에 원 두 개를 최소제곱으로 맞춰 나온 수치다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Guard", meta = (ForceUnits = "px"))
	FVector2D OuterCenter = FVector2D(114.25, 96.5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Guard", meta = (ClampMin = "1", ForceUnits = "px"))
	float OuterRadius = 84.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Guard", meta = (ForceUnits = "px"))
	FVector2D InnerCenter = FVector2D(87.0, 96.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Guard", meta = (ClampMin = "1", ForceUnits = "px"))
	float InnerRadius = 84.38f;

	/**
	 * 울타리를 바깥으로 벌리는 폭. 모양은 그림이 정하므로 울타리는 초승달을 넉넉히 감싸기만 하면 된다.
	 * 너무 좁으면 초승달의 진짜 가장자리를 잘라먹고, 너무 넓으면 아이콘의 다른 흰 점이 새어 들어온다.
	 *
	 * 게이지가 차오르는 세로 범위는 이 여유를 빼고 잰다 — 넓히면 다 찼는데도 위쪽이 남는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Guard", meta = (ClampMin = "0", ForceUnits = "px"))
	float GuardMargin = 5.0f;

	//~ 액체

	/** 값이 바뀔 때 게이지가 따라가는 시간. 한 발 쏠 때마다 계단처럼 뚝 떨어지지 않게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Look", meta = (ClampMin = "0", ForceUnits = "s"))
	float FillSmoothingSeconds = 0.15f;

	/**
	 * 액체 색을 내 팀 색(TeamLook, MPC_TeamLook)으로 할지. 끄면 LiquidColor를 그대로 쓴다.
	 * 관전하거나 팀이 아직 없으면 어느 쪽이든 LiquidColor다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Look")
	bool bUseTeamColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Look")
	FLinearColor LiquidColor = FLinearColor(FColor(120, 220, 205));

	/**
	 * 위에서 아래로 겹치는 물결 세 겹. 커버리지 바와 같은 구조체다.
	 *
	 * Height는 액체 윗면(0)에서 바닥(1)까지의 비율이라, 물결선이 게이지가 오르내리는 대로 따라
	 * 움직인다. Waves[0]은 액체의 윗면 그 자체다.
	 */
	UPROPERTY(EditAnywhere, Category = "Paint Gauge|Waves")
	FPaintBarWave Waves[3];

	/** Waves[0]선 위쪽의 불투명도. 0이면 액체 윗면이 물결 모양으로 잘려 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint Gauge|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float TopLayerOpacity = 0.0f;

	/** 위젯 디자이너에서만 쓰는 값. 게임에서는 무시한다. */
	UPROPERTY(EditAnywhere, Category = "Paint Gauge|Preview", meta = (ClampMin = "0", ClampMax = "1"))
	float DesignerFill = 0.65f;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootBox;

private:
	/** 위젯 디자이너의 미리보기인지. 여기에는 폰이 없으므로 DesignerFill을 쓴다. */
	bool IsPreview() const;

	/** 이 화면 주인의 잉크(0~1). 유닛이나 잉크 통이 없으면 -1: 그리지 않는다. */
	float ReadInk() const;

	FLinearColor GetLiquidColor() const;

	void UpdateMaterial();
	bool EnsureMaterialInstance();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GaugeMaterialInstance;

	FSlateBrush GaugeBrush;

	/** 따라가는 중인 채움과, 한 번이라도 읽었는지. 처음 값은 보간 없이 그 자리에서 시작한다. */
	float DisplayedFill = 0.0f;
	bool bHasFill = false;

	bool bFillOverrideEnabled = false;
	float FillOverride = 0.0f;

	float WaveTime = 0.0f;
};
