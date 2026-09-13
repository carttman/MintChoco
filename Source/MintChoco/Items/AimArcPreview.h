#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AimArcPreview.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * 던지는 아이템의 조준 궤적 표시. 점을 일정 간격으로 늘어놓아 포물선을 그리고, 끝에 착탄
 * 지점을 표시한다.
 *
 * 조준하는 본인 화면에만 존재한다(UItemAimAbility가 로컬에서만 스폰한다). 그래서 복제하지
 * 않고 충돌도 갖지 않는다 — 내 화면의 표시가 남의 이동을 막으면 서버와 어긋난다.
 *
 * 메시와 재질은 서브클래스 BP가 지정한다. 크기 값들은 기본 도형(한 변 100 cm)을 전제로
 * 하므로, 다른 크기의 메시를 물리면 그만큼 어긋난다.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API AAimArcPreview : public AActor
{
	GENERATED_BODY()

public:
	AAimArcPreview();

	/**
	 * 궤적을 갱신한다. Path는 예측이 내놓은 원본 점들이고, 화면에 찍을 점은 DotSpacing 간격으로
	 * 다시 뽑는다. 예측의 시간 간격을 그대로 쓰면 빠른 구간은 성기고 느린 구간은 뭉친다.
	 *
	 * bHasImpact가 거짓이면 착탄 표시를 숨긴다(예측 시간 안에 아무것도 맞히지 못했다).
	 */
	void SetArc(const TArray<FVector>& Path, bool bHasImpact, const FVector& ImpactPoint, const FVector& ImpactNormal);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
	TObjectPtr<UInstancedStaticMeshComponent> Dots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
	TObjectPtr<UStaticMeshComponent> Impact;

	/** 점 하나의 지름(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1", ForceUnits = "cm"))
	float DotSize = 14.0f;

	/** 점 사이 거리(cm). 좁을수록 선에 가까워지고 그만큼 점이 늘어난다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1", ForceUnits = "cm"))
	float DotSpacing = 60.0f;

	/** 착탄 표시의 지름(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1", ForceUnits = "cm"))
	float ImpactSize = 150.0f;

	/** 착탄 표시의 두께(cm). 면에 붙은 판으로 보이도록 얇게. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float ImpactThickness = 4.0f;

private:
	/** 기본 도형의 한 변. 스케일 1이 100 cm다. */
	static constexpr float ShapeSize = 100.0f;

	/** 아무리 멀리 던져도 점이 이만큼까지만 찍힌다. */
	static constexpr int32 MaxDots = 256;

	/** 매 프레임 다시 할당하지 않도록 들고 있는다. */
	TArray<FTransform> DotTransforms;
};
