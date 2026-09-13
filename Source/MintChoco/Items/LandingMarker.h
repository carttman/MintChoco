#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LandingMarker.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * 히어로 랜딩의 착지점 표시. 원 두 개를 겹쳐 그린다.
 *
 * - 바깥 원: 끝까지 버텼을 때의 효과 반경. 크기가 변하지 않아 "최대 이만큼"을 말한다.
 * - 안쪽 원: 지금 놓으면 나오는 반경. 호버를 버티는 동안 자라 바깥 원을 향해 간다.
 *
 * 조준하는 본인 화면에만 존재한다(어빌리티가 로컬에서만 스폰한다). 그래서 복제하지 않고
 * 충돌도 갖지 않는다 — 내 화면의 표시가 남의 이동을 막으면 서버와 어긋난다.
 *
 * AAimArcPreview와 같은 규칙으로, 메시와 재질은 서브클래스 BP가 지정한다. 반지름 계산은
 * 지름 100 cm짜리 기본 원기둥을 전제로 한다.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API ALandingMarker : public AActor
{
	GENERATED_BODY()

public:
	ALandingMarker();

	/**
	 * 끝까지 버텼을 때의 반경(cm). 어빌리티가 스폰 직후 한 번 넣는다. 바깥 원이 이 크기로
	 * 서고, 안쪽 원은 이 값에 충전량을 곱한 크기까지 자란다.
	 */
	void SetMaxRadius(float NewMaxRadius);

	/**
	 * 지금 충전량(0~1)을 반영한다. 무브먼트가 시간으로 세는 값이라 매 프레임 들어온다.
	 *
	 * 반경이 0부터 시작하지 않는 이유: 아무리 일찍 눌러도 효과가 아예 없지는 않다.
	 * MinRadiusFraction이 그 바닥값이고, 어빌리티의 MinChargeScale과 같은 값을 넣어야
	 * 표시와 실제가 맞는다.
	 */
	void SetCharge(float NewCharge);

protected:
	/**
	 * 스케일이 없는 루트. 두 원이 여기 붙는다.
	 *
	 * 원 하나를 루트로 삼으면 그 스케일이 다른 원에 곱해져, 작은 원을 줄일수록 두께까지
	 * 같이 눌린다. 두 크기가 서로 무관해야 하므로 루트는 크기를 갖지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<USceneComponent> Origin;

	/** 크기가 변하지 않는 바깥 원. 최대 반경을 말한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> MaxRing;

	/** 충전량만큼 자라는 안쪽 원. 겹치는 만큼 진해져 게이지로 읽힌다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> ChargeRing;

	/** 두 원이 함께 쓰는 메시. 지름 100 cm짜리 원기둥을 전제로 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMesh> RingMesh;

	/** 두 원이 함께 쓰는 재질. 반투명이어야 겹친 부분이 진해져 게이지로 보인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UMaterialInterface> RingMaterial;

	/** 원의 두께(cm). 바닥에 붙은 판으로 보이도록 얇게. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float RingThickness = 4.0f;

	/** 충전 0에서의 반경 비율. 어빌리티의 MinChargeScale과 같은 값이어야 표시가 맞는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker", meta = (ClampMin = "0", ClampMax = "1"))
	float MinRadiusFraction = 0.5f;

	/**
	 * 안쪽 원을 바깥 원보다 이만큼 띄운다(cm). 같은 높이에 두면 두 면이 서로 번갈아 이겨
	 * 깜빡인다(z-fighting).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marker", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float ChargeRingLift = 2.0f;

	virtual void BeginPlay() override;

private:
	/** 두 원의 크기를 지금 값으로 다시 맞춘다. */
	void RefreshRings();

	/** 기본 원기둥의 지름. 스케일 1이 100 cm다. */
	static constexpr float ShapeDiameter = 100.0f;

	float MaxRadius = 400.0f;
	float Charge = 0.0f;
};
