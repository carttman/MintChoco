#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "PaintCrosshairWidget.generated.h"

class UPaintWeaponComponent;

/** 크로스헤어 표시의 순수 계산. 테스트가 월드 없이 검사한다. */
struct MINTCHOCO_API FPaintCrosshairMath
{
	/**
	 * 예상 탄착이 조준점보다 시선 방향으로 ShortfallCm 이상 앞에 있는지. 마커를 띄우는 조건은
	 * "달라졌다"가 아니라 "가까이 떨어진다"이다: 공이 조준점까지 못 가고 중력에 떨어지거나 더
	 * 가까운 것에 먼저 맞는 경우. 같은 점이거나 더 멀면 거짓.
	 */
	static bool ImpactFallsShort(const FVector& ViewOrigin, const FVector& ViewDirection, const FVector& AimPoint, const FVector& Impact, float ShortfallCm);

	/** 중앙 원 반지름: 사격 블렌드에 따라 기본에서 FiringScale 배까지. */
	static float CenterRadius(float BaseRadius, float FiringScale, float FiringBlend);

	/** 중앙 원 알파: 기본↔사격을 블렌드한 뒤, 마커가 보이는 만큼 DimmedOpacity 배로 눌러 반투명하게. */
	static float CenterOpacity(float IdleOpacity, float FiringOpacity, float FiringBlend, float MarkerBlend, float DimmedOpacity);
};

/**
 * 크로스헤어의 공통 기계. 모양은 자식이 PaintReticle 로 그리고, 여기는 무기 하나를 받아
 * 사격 상태·예상 탄착 마커·등장 페이드를 계산해 준다. 텍스처 없이 NativePaint 로 그린다
 * (UPaintChargeWidget 과 같은 방식). 풀스크린 캔버스 슬롯에 놓는 것을 전제로 중앙을 자기
 * 크기의 절반으로 잡고, 마커는 월드 좌표를 위젯 좌표로 투영해 놓는다.
 *
 * 어느 무기를 보여 줄지는 UPaintCrosshairHostWidget 이 정해 SetWeapon 으로 넣어 준다(주무기, 또는
 * 방아쇠를 당긴 보조무기). 무기 프로필이 자기 크로스헤어 클래스를 고르므로(UPaintWeaponProfile::
 * CrosshairClass) 무기마다 다른 자식이 뜬다.
 *
 * - 사격: 방아쇠를 당기고 있거나(충전 포함) 방금 쐈으면 FiringBlend 가 1로 간다.
 * - 예상 탄착: 무기가 예측한 다음 발의 탄착이 조준점보다 앞에 떨어지면 주황+흰색 마커가 그 화면
 *   위치로 간다. 히트스캔·브러시는 예측이 없어 마커가 없다.
 * - 등장: SetShown 으로 Presence 가 0↔1 로 흐르고 모든 알파에 곱한다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UPaintCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 상태를 읽을 무기. 바뀌면 발사 알림을 옛 무기에서 풀고 새 무기에 건다. null 이면 기본 상태만 그린다. */
	void SetWeapon(UPaintWeaponComponent* Weapon);

	/** 보이기/숨기기. 즉시 켜지는 것이 아니라 PresenceSpeed 로 페이드한다. */
	void SetShown(bool bShown);

	/** 숨기라고 했고 페이드까지 끝났는지. 호스트가 이때 Collapsed 로 접는다. */
	bool IsFadedOut() const { return !bWantsShown && Presence <= UE_KINDA_SMALL_NUMBER; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/**
	 * 자식이 제 모양을 그린다. Center 는 위젯 중앙, Alpha 는 등장 페이드(모든 색의 알파에 곱할
	 * 것). 마지막으로 쓴 레이어를 돌려주면 베이스가 그 위에 탄착 마커를 얹는다.
	 */
	virtual int32 PaintReticle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer, const FVector2f& Center, float Alpha) const
	{
		return Layer;
	}

	const UPaintWeaponComponent* GetWeapon() const { return BoundWeapon.Get(); }
	float GetFiringBlend() const { return FiringBlend; }
	float GetMarkerBlend() const { return MarkerBlend; }
	float GetPresence() const { return Presence; }

	/** mc.CrosshairPreview: 음수면 실제 상태, 0 기본, 1 사격, 2 마커. 자식이 미리보기에 맞춰 그릴 때 읽는다. */
	static int32 GetPreviewState();

	/** 원 둘레 점들. 세그먼트 수는 Segments. */
	void BuildCircle(const FVector2f& Center, float Radius, TArray<FVector2f>& OutPoints) const;

	/** 링 + 점 한 벌. GlowLayers 만큼 아래에 더 굵고 옅은 링을 깐다(차지 링과 같은 방식). */
	void DrawRing(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& AllottedGeometry,
		const FVector2f& Center, float Radius, float Thickness, float DotRadius, const FLinearColor& Color, float Opacity, int32 GlowLayers, float GlowAlpha) const;

	/** 사격 상태로 드나드는 속도(FInterpTo). 클수록 즉시. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "1"))
	float FiringBlendSpeed = 14.0f;

	/** 한 발 나간 뒤 사격 상태를 유지하는 시간. 단발 클릭도 이만큼은 밝다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "0", ForceUnits = "s"))
	float FireFlashSeconds = 0.15f;

	/** 나타나고 사라지는 속도(FInterpTo). 12면 0.1초 남짓. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "1"))
	float PresenceSpeed = 12.0f;

	/** 원 하나의 세그먼트 수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "8"))
	int32 Segments = 48;

	//~ 예상 탄착 마커

	/** 바깥 주황 링. 차지 링(1, 0.78, 0.1)보다 붉게 두어 둘이 구분된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker")
	FLinearColor MarkerColor = FLinearColor(1.0f, 0.55f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "1", ForceUnits = "px"))
	float MarkerRadius = 13.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float MarkerThickness = 3.0f;

	/** 안쪽 흰 링. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "1", ForceUnits = "px"))
	float MarkerInnerRadius = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "0.5", ForceUnits = "px"))
	float MarkerInnerThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "0", ForceUnits = "px"))
	float MarkerDotRadius = 3.0f;

	/** 마커 글로우의 알파. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "0", ClampMax = "1"))
	float MarkerGlowStrength = 0.35f;

	/** 탄착이 조준점보다 시선 방향으로 이만큼 이상 앞에 있어야 마커가 뜬다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MarkerShortfallCm = 50.0f;

	/** 마커가 나타나고 사라지는 속도(FInterpTo). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "1"))
	float MarkerBlendSpeed = 12.0f;

	/**
	 * 마커 위치를 따라가는 속도(Vector2DInterpTo). 총구 소켓이 달리기 애니메이션에 흔들려 원거리
	 * 탄착이 떨리는 것을 눌러 준다. 처음 나타날 때는 스무딩 없이 제자리에 뜬다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair|Marker", meta = (ClampMin = "1"))
	float MarkerInterpSpeed = 30.0f;

private:
	UFUNCTION()
	void HandleWeaponFired(int32 Seed);

	TWeakObjectPtr<UPaintWeaponComponent> BoundWeapon;

	/** 이 머신의 월드 시계로 잰 마지막 발사 시각. 음수면 아직 없다. */
	double LastFiredTime = -1.0;

	bool bWantsShown = true;
	float Presence = 0.0f;
	float FiringBlend = 0.0f;
	float MarkerBlend = 0.0f;
	FVector2f MarkerLocal = FVector2f::ZeroVector;
};
