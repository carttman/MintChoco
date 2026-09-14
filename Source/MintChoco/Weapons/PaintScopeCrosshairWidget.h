#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintCrosshairWidget.h"

#include "PaintScopeCrosshairWidget.generated.h"

/**
 * 차지 무기(스니퍼)의 스코프형 크로스헤어: 얇은 큰 원, 상하좌우 눈금 넷, 중앙 점. 차지 링
 * (UPaintChargeWidget, 반지름 28)은 따로 있으므로 그 바깥에 그린다. 충전이 가득 차면 눈금과 원이
 * 차지 링과 같은 금색으로 물들어 "지금 놓아라"를 한 번 더 말한다.
 */
UCLASS()
class MINTCHOCO_API UPaintScopeCrosshairWidget : public UPaintCrosshairWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 PaintReticle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2f& Center, float Alpha) const override;

	/** 바깥 원의 반지름. 차지 링(28)보다 넉넉히 커야 둘이 겹치지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "1", ForceUnits = "px"))
	float ScopeRadius = 46.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float ScopeThickness = 1.5f;

	/** 눈금이 시작하는 반지름. 원까지 뻗는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "0", ForceUnits = "px"))
	float TickInnerRadius = 36.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float TickThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "0", ForceUnits = "px"))
	float DotRadius = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope")
	FLinearColor ScopeColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	/** 충전이 가득 찼을 때의 색. 차지 링의 색과 같다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope")
	FLinearColor ChargedColor = FLinearColor(1.0f, 0.78f, 0.1f, 1.0f);

	/** 금색으로 물드는 속도(FInterpTo). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "1"))
	float ChargedBlendSpeed = 10.0f;

	/** 사격 중(홀드 중) 전체가 커지는 배율. 1이면 그대로. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Scope", meta = (ClampMin = "1", ClampMax = "2", ForceUnits = "x"))
	float FiringScale = 1.1f;

private:
	float ChargedBlend = 0.0f;
};
