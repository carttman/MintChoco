#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "InvisibleWall.generated.h"

class UBillboardComponent;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * 플레이어만 막는 보이지 않는 벽. 맵 경계나 못 가게 할 곳에 놓는다.
 *
 * 박스 하나가 전부다. 폰(캡슐)만 Block이고 나머지는 전부 Ignore라 카메라, 시야·조준 트레이스,
 * 페인트탄, 스나이퍼 광선, 아이템 투사체(꿀풍선·꿀벌)는 그대로 지나간다. 로직도 복제도 없다:
 * 레벨에 놓인 액터라 모든 머신에 같은 자리에 있고, 서버와 클라이언트가 같은 벽에 막힌다.
 *
 * 에디터에서는 반투명 박스 메시와 아이콘으로 보인다. 둘 다 에디터 전용 컴포넌트라 게임
 * 월드에는 만들어지지 않는다. 크기는 액터 스케일로 잡는다(기본 100×20×300 cm).
 */
UCLASS()
class MINTCHOCO_API AInvisibleWall : public AActor
{
	GENERATED_BODY()

public:
	AInvisibleWall();

protected:
	/** 막는 박스. 폰만 Block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall")
	TObjectPtr<UBoxComponent> Box;

#if WITH_EDITORONLY_DATA
	/** 에디터에서만 보이는 반투명 박스. 블루프린트가 메시와 재질을 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall")
	TObjectPtr<UStaticMeshComponent> Preview;

	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
