#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ItemSpawnPoint.generated.h"

class AItemPickup;
class UBillboardComponent;

/**
 * 맵에 배치하는 아이템 스폰 지점. 표식일 뿐 로직은 없다.
 *
 * 복제하지 않는다. 레벨에 놓인 액터라 모든 머신에 이미 있고, 여기서 무엇이 나올지는
 * 서버의 게임모드가 정해 픽업 액터로 복제한다. CurrentPickup은 서버만 채운다.
 */
UCLASS()
class MINTCHOCO_API AItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AItemSpawnPoint();

	/** 서버 전용. 지금 이 자리에 아이템이 놓여 있지 않은지. */
	bool IsFree() const { return !CurrentPickup.IsValid(); }

	/** 서버 전용. 여기 놓인 픽업. 픽업이 사라지면 약한 참조가 알아서 풀린다. */
	TWeakObjectPtr<AItemPickup> CurrentPickup;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
