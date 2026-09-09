#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ItemPickup.generated.h"

class AItemSpawnPoint;
class UItemProfile;
class USphereComponent;
class UStaticMeshComponent;

/** 픽업의 두 단계. 예고 중에는 레이저만 보이고, 활성화되면 아이템이 놓이고 밟을 수 있다. */
UENUM(BlueprintType)
enum class EItemPickupState : uint8
{
	Announced,
	Active
};

/**
 * 맵에 놓인 아이템. 서버가 스폰하고 복제한다.
 *
 * 스폰 직후에는 예고 상태로 수직 레이저만 세우고, WarningTime 뒤에 아이템이 나타난다.
 * 캐릭터의 캡슐이 닿는 순간 서버가 슬롯에 넣고 액터를 치운다. 습득 판정은 서버만 하므로
 * 두 플레이어가 동시에 밟아도 한 명만 가져간다. 클라이언트는 사라지는 것을 복제로 본다.
 */
UCLASS()
class MINTCHOCO_API AItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AItemPickup();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. SpawnActorDeferred 뒤, FinishSpawning 전에 부른다. */
	void Initialize(UItemProfile* InProfile, AItemSpawnPoint* InSpawnPoint, float InWarningTime);

	UFUNCTION(BlueprintPure, Category = "Item")
	UItemProfile* GetProfile() const { return Profile; }

	UFUNCTION(BlueprintPure, Category = "Item")
	EItemPickupState GetState() const { return State; }

	/** 상태가 바뀔 때, 모든 머신에서. BP가 레이저·등장 연출을 여기에 붙인다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item")
	void BP_OnStateChanged(EItemPickupState NewState);

protected:
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_Profile, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UItemProfile> Profile;

	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Item")
	EItemPickupState State = EItemPickupState::Announced;

	/** 누가 가져갔다. 잠깐 뒤 사라지는 사이 클라이언트가 소리를 낼 수 있게 복제한다. */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_Collected, Category = "Item")
	bool bCollected = false;

	/** 예고 시간(초). 서버가 Initialize로 넣는다. */
	float WarningTime = 5.0f;

	/** 발동 범위. 활성 상태에서만 켜진다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USphereComponent> Trigger;

	/** 아이템 외형. 프로필의 PickupMesh. 활성 상태에서만 보인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 예고용 수직 레이저. 예고 상태에서만 보인다. 메시와 머티리얼은 BP가 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Laser;

	UFUNCTION()
	void OnRep_Profile();

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Collected();

private:
	void ApplyProfile();
	void ApplyState();
	void Activate();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 서버 전용. 이 픽업이 차지한 지점. 사라질 때 비워 준다. */
	TWeakObjectPtr<AItemSpawnPoint> SpawnPoint;

	FTimerHandle ActivateTimer;
};
