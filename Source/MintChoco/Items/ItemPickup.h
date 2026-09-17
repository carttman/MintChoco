#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ItemPickup.generated.h"

class AItemSpawnPoint;
class UItemProfile;
class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;
class UStaticMeshComponent;
class UUserWidget;
class UWidgetComponent;

/** 픽업의 두 단계. 예고 중에는 레이저만 보이고, 활성화되면 아이템이 놓이고 밟을 수 있다. */
UENUM(BlueprintType)
enum class EItemPickupState : uint8
{
	Announced,
	Active
};

/** 박스 메시의 회전. 상하 흔들림은 풍선과 함께 쓰는 BobMotion::Offset이 맡는다. */
struct MINTCHOCO_API FItemPickupMotion
{
	/** 누적 요(도), -180~180으로 정규화. 시간에서 바로 구하므로 dt 오차가 쌓이지 않는다. */
	static float SpinYaw(float Time, float RateDegPerSecond);
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
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. SpawnActorDeferred 뒤, FinishSpawning 전에 부른다. */
	void Initialize(UItemProfile* InProfile, AItemSpawnPoint* InSpawnPoint, float InWarningTime);

	/**
	 * 서버 전용. 지점 위치에 픽업을 지연 스폰해 초기화하고 끝낸다. 게임모드와 Standalone 지점이 같은 길을 쓴다.
	 * WarningTime 0이면 바로 활성 상태로 나온다. 스폰이 거부되면 nullptr.
	 */
	static AItemPickup* SpawnAt(UWorld& World, UClass* PickupClass, AItemSpawnPoint& Point, UItemProfile& Item, float WarningTime, AActor* Owner);

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

	/**
	 * 활성 상태의 박스에 붙는 오라. 비어 있으면 오라가 없다.
	 *
	 * 메시에 붙으므로 박스가 떠다니고 도는 것을 그대로 따라간다. 상태는 복제되므로 모든
	 * 머신에서 같이 켜지고 꺼진다. 아이템 종류와 무관하게 같은 오라라 프로필이 아니라
	 * 클래스 디폴트에 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UNiagaraSystem> AuraFX;

	/** 그 오라. 지속되는 이펙트라 상태가 풀릴 때 직접 꺼야 하므로 들고 있는다. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AuraFXComponent;

	/** 아이템 위에 뜨는 디버그 이름표(스크린 공간). PIE에서 bShowLabel이 켜져 있고 활성 상태일 때만 보인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Label")
	TObjectPtr<UWidgetComponent> Label;

#if WITH_EDITORONLY_DATA
	/** PIE에서 이름표를 띄운다. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Label")
	bool bShowLabel = false;
#endif

	/** 이름표 위젯. 기본은 트리를 직접 짜는 UItemLabelWidget이고, UMG 서브클래스로 바꿀 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Label")
	TSubclassOf<UUserWidget> LabelWidgetClass;

	/** 이름표가 뜨는 높이(cm). 박스 메시 위쪽. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Label", meta = (ForceUnits = "cm"))
	float LabelHeight = 150.0f;

	/**
	 * 활성 상태의 박스 메시 연출. 복제하지 않고 머신마다 돌린다: 메시의 상대 위치를 위아래로 흔들고
	 * 상대 요를 계속 돌린다. BP가 정한 상대 트랜스폼이 기준이고, 습득 판정(Trigger)은 움직이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Motion", meta = (ClampMin = "0", ForceUnits = "cm"))
	float BobAmplitude = 10.0f;

	/** 상하 왕복 횟수(초당). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Motion", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float BobFrequency = 1.0f;

	/** 요 회전 속도(도/초). 양수가 위에서 봤을 때 시계 방향(오른쪽). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Motion", meta = (ForceUnits = "deg/s"))
	float SpinRateDeg = 90.0f;

	UFUNCTION()
	void OnRep_Profile();

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Collected();

private:
	void ApplyProfile();
	void ApplyState();

	/** 활성 상태에 맞춰 오라를 켜고 끈다. 데디케이티드 서버는 지나간다. */
	void UpdateAura(bool bActive);
	void Activate();
	void UpdateLabel();
	bool IsLabelEnabled() const;

	/** 활성이고 아직 아무도 가져가지 않았을 때만 틱(연출)이 돈다. */
	void UpdateMotionEnabled();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 서버 전용. 이 픽업이 차지한 지점. 사라질 때 비워 준다. */
	TWeakObjectPtr<AItemSpawnPoint> SpawnPoint;

	FTimerHandle ActivateTimer;

	/** 연출 시계(초)와 BP가 정한 메시의 기준 상대 트랜스폼. BeginPlay에서 읽는다. */
	float MotionTime = 0.0f;
	FVector MeshBaseLocation = FVector::ZeroVector;
	FRotator MeshBaseRotation = FRotator::ZeroRotator;
};
