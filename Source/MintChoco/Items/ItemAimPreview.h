#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"

#include "ItemAimPreview.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** 궤적 미리보기의 모양. 아이템 프로필이 들고 있고, 미리보기 액터가 그대로 그린다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FItemAimPreviewStyle
{
	GENERATED_BODY()

	/**
	 * 궤적을 이루는 점 하나. 비어 있으면 개발 빌드에서 디버그 선으로 대신 그린다(출하 빌드에서는
	 * 아무것도 보이지 않는다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	TObjectPtr<UStaticMesh> DotMesh;

	/** 비어 있으면 메시의 재질을 그대로 쓴다. 팀 색을 입히려면 여기에 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	TObjectPtr<UMaterialInterface> DotMaterial;

	/** 점 사이의 간격(cm). 궤적을 따라 잰 거리다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "5", ForceUnits = "cm"))
	float DotSpacing = 70.0f;

	/** 점 하나의 지름(cm). 메시의 원래 크기와 무관하게 이 크기로 맞춰진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1", ForceUnits = "cm"))
	float DotSize = 12.0f;

	/** 착탄 지점 표시. 히어로 랜딩의 AimMarkerClass와 같은 용도이고, 비어 있어도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	TSubclassOf<AActor> ImpactMarkerClass;

	/** 궤적을 몇 초 앞까지 그릴지. 실제 투사체는 이보다 오래 날 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxSimTime = 3.0f;
};

/**
 * 던질 물건이 그릴 곡선을 보여 주는 표시. 조준하는 본인의 화면에만 존재한다.
 *
 * 복제하지 않고 충돌도 없다: 이 액터는 이 머신에만 있으므로, 무엇이든 막으면 내 화면에서만
 * 남이 밀리거나 걸려 서버와 어긋난다(히어로 랜딩의 착지점 표시와 같은 이유다).
 *
 * 곡선은 UGameplayStatics::PredictProjectilePath가 낸다. 실제 투사체와 같은 출발점·속도·
 * 중력 배율·반지름을 넣으므로, 표시된 자리와 터지는 자리가 같다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API AItemAimPreview : public AActor
{
	GENERATED_BODY()

public:
	AItemAimPreview();

	/** 이 머신에만 만든다. 소유자는 표시를 띄운 폰. */
	static AItemAimPreview* Spawn(UWorld& World, AActor* Owner, const FItemAimPreviewStyle& InStyle);

	/**
	 * 궤적을 다시 계산해 점과 착탄 표시를 옮긴다. 매 프레임 불린다.
	 *
	 * Radius는 투사체의 구 반지름이다. 0이면 선으로 훑으므로 모서리를 스쳐 지나간다.
	 */
	void UpdateArc(const FVector& Start, const FVector& Velocity, float GravityScale, float Radius, const AActor* IgnoreActor);

	virtual void Destroyed() override;

private:
	/** 착탄 표시를 필요할 때 만든다. ImpactMarkerClass가 비어 있으면 만들지 않는다. */
	void UpdateImpactMarker(const FVector& Location, bool bHit);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Dots;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ImpactMarker;

	UPROPERTY(Transient)
	FItemAimPreviewStyle Style;

	/** 곡선을 훑을 때 무엇을 막는 것으로 볼지. 생성자에서 한 번 채운다. */
	TArray<TEnumAsByte<EObjectTypeQuery>> BlockingObjects;

	/** 점 자리를 매 프레임 새로 담는 버퍼. 할당을 반복하지 않으려고 멤버로 둔다. */
	TArray<FTransform> DotTransforms;
};
