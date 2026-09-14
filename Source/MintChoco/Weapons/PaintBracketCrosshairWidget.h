#pragma once

#include "CoreMinimal.h"

#include "Weapons/PaintCrosshairWidget.h"

#include "PaintBracketCrosshairWidget.generated.h"

/**
 * 기본 크로스헤어: 가로로 넓은 직사각형의 네 귀퉁이 브래킷과 중앙 원. 브래킷은 상태와 무관하게
 * 고정이고, 중앙 원이 기본(회색)↔사격(흰색, 소폭 확대)을 오가며 마커가 보이면 반투명해진다.
 * 프로필이 크로스헤어를 고르지 않은 무기는 이것을 쓴다.
 */
UCLASS()
class MINTCHOCO_API UPaintBracketCrosshairWidget : public UPaintCrosshairWidget
{
	GENERATED_BODY()

protected:
	virtual int32 PaintReticle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2f& Center, float Alpha) const override;

	//~ 브래킷

	/** 중앙에서 브래킷 꼭짓점까지 가로·세로 거리. 가로가 더 커서 직사각형이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Bracket", meta = (ForceUnits = "px"))
	FVector2D BracketHalfExtent = FVector2D(80.0, 40.0);

	/** 꼭짓점에서 뻗는 두 팔의 길이. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Bracket", meta = (ClampMin = "1", ForceUnits = "px"))
	float BracketArm = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Bracket", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float BracketThickness = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Bracket")
	FLinearColor BracketColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	//~ 중앙 원

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "1", ForceUnits = "px"))
	float CenterRadius = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float CenterThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ForceUnits = "px"))
	float CenterDotRadius = 3.0f;

	/** 비사격 상태의 색. 알파는 IdleOpacity 가 따로 정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center")
	FLinearColor IdleColor = FLinearColor(0.75f, 0.75f, 0.75f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ClampMax = "1"))
	float IdleOpacity = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center")
	FLinearColor FiringColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ClampMax = "1"))
	float FiringOpacity = 1.0f;

	/** 사격 중 중앙 원의 크기 배율. 1이면 밝기만 바뀐다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "1", ClampMax = "2", ForceUnits = "x"))
	float FiringScale = 1.3f;

	/** 사격 중 원 아래 깔리는 글로우 겹 수. 0이면 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ClampMax = "4"))
	int32 FiringGlowLayers = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.35f;

	/** 마커가 보일 때 중앙 원 알파에 곱하는 값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Center", meta = (ClampMin = "0", ClampMax = "1"))
	float CenterDimmedOpacity = 0.35f;
};
