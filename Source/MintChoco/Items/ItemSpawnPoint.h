#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ItemSpawnPoint.generated.h"

class AItemPickup;
class UBillboardComponent;
class UItemProfile;

/** 이 지점에 아이템이 놓이는 방식. */
UENUM(BlueprintType)
enum class EItemSpawnMode : uint8
{
	/** 게임모드가 주기마다 빈 지점 하나를 골라 놓는다. 실전 맵의 기본. */
	Shared,
	/** 아이템 지정: 시작하자마자 놓이고, 가져가면 RespawnDelay 뒤 다시 놓인다. 테스트 맵용. */
	Standalone
};

/**
 * 맵에 배치하는 아이템 스폰 지점.
 *
 * 복제하지 않는다. 레벨에 놓인 액터라 모든 머신에 이미 있고, 여기서 무엇이 나올지는
 * 서버가 정해 픽업 액터로 복제한다. CurrentPickup은 서버만 채운다.
 *
 * Shared 지점은 표식일 뿐이고 게임모드가 지정한다. Standalone 지점은 게임모드 목록에서 빠지고
 * 자기 픽업을 직접 놓는다. FixedItem을 채우면 어느 모드든 늘 그 아이템, 비워 두면 설정 목록에서 무작위.
 */
UCLASS()
class MINTCHOCO_API AItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AItemSpawnPoint();

	/** 서버 전용. 지금 이 자리에 아이템이 놓여 있지 않은지. */
	bool IsFree() const { return !CurrentPickup.IsValid(); }

	EItemSpawnMode GetSpawnMode() const { return SpawnMode; }

	/** 이 자리에서 나올 아이템. 없으면 무작위. */
	UItemProfile* GetFixedItem() const { return FixedItem; }

	/** 고정 아이템이 있으면 그것, 없으면 설정 목록에서 무작위. 둘 다 없으면 nullptr. */
	UItemProfile* ChooseItem(const FRandomStream& Random) const;

	/** 서버 전용. 여기 놓인 픽업. 픽업이 사라지면 약한 참조가 알아서 풀린다. */
	TWeakObjectPtr<AItemPickup> CurrentPickup;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Item")
	EItemSpawnMode SpawnMode = EItemSpawnMode::Shared;

	/** 이 자리에 고정할 아이템. 비워 두면 설정의 목록에서 무작위로 고른다. */
	UPROPERTY(EditAnywhere, Category = "Item")
	TObjectPtr<UItemProfile> FixedItem;

	/** Standalone: 가져간 뒤 다음 아이템이 나오기까지(초). 첫 아이템은 즉시 + 레이저 예고 */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "SpawnMode == EItemSpawnMode::Standalone"))
	float RespawnDelay = 3.0f;

private:
	/** 서버 전용. 픽업 하나를 놓고, 사라지면 다시 놓도록 건다. */
	void SpawnStandalone(float WarningTime);

	UFUNCTION()
	void HandlePickupDestroyed(AActor* DestroyedActor);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
