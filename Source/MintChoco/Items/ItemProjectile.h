#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/TeamTypes.h"

#include "ItemProjectile.generated.h"

class AUnit;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 아이템이 날리는 투사체의 공통 골격(꿀풍선, 꿀벌). 페인트탄(APaintProjectile)과 달리 복제 액터다.
 *
 * 페인트탄은 머신마다 자기 공을 날리고 시드로 맞추지만, 이 투사체는 상대를 맞혀 상태를 걸고
 * (서버만 판정) 꿀벌처럼 조종되기도 해서 결정적으로 재현할 수 없다. 그래서 서버의 것 하나가
 * 진실이고 이동을 복제한다. 클라이언트 복사본은 콜리전 없이 마지막으로 받은 속도로 날다가
 * 넷 업데이트마다 메시를 보간해 따라간다(PostNetReceive*). 판정과 효과는 서버만 한다.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API AItemProjectile : public AActor
{
	GENERATED_BODY()

public:
	AItemProjectile();

	/** SpawnActorDeferred와 FinishSpawning 사이에. 무브먼트가 초기화될 때 속도를 읽는다. */
	void Init(AUnit* InInstigator, int32 InTeam, const FVector& Velocity);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PostNetReceiveLocationAndRotation() override;

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetTeam() const { return Team; }

	/** 팀이 곧 페인트 id. 팀이 없으면 0. */
	uint8 GetPaintId() const { return Teams::IsValidId(Team) ? static_cast<uint8>(Team) : 0; }

	/** 던진 유닛. Instigator와 같지만 타입이 맞춰져 있다. */
	AUnit* GetInstigatorUnit() const;

	/** 충돌 구의 반지름(cm). 궤적 미리보기가 같은 굵기로 훑는다. CDO에서도 읽을 수 있다. */
	float GetCollisionRadius() const;

	/**
	 * 이 머신이 이 이펙트를 그려야 하는가. 데디케이티드 서버는 그림을 그리지 않고, 슬롯이 비어
	 * 있으면 어느 머신에서도 아무 일이 없다. 판단만 하므로 월드 없이도 부를 수 있다.
	 */
	static bool ShouldShowFX(ENetMode NetMode, const UNiagaraSystem* FX);

protected:
	/** 서버 전용. 월드(벽, 바닥)에 막혔다. 기본은 터진다. */
	virtual void HandleWorldHit(const FHitResult& Hit);

	/** 서버 전용. 유닛에 닿았다(사용자 본인은 이미 걸러졌다). 기본은 아무것도 안 한다. */
	virtual void HandleUnitOverlap(AUnit& Unit) {}

	/** 서버 전용. 효과를 내고 사라진다. 두 번 불려도 한 번만 터진다. */
	void Detonate();

	/** 서버 전용. 실제 효과. 서브클래스가 채운다. 이 뒤에 Destroy가 온다. */
	virtual void OnDetonate() {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/**
	 * 날아가는 동안 뒤에 남는 이펙트. 비워 두면 트레일이 없다(꿀벌이 그렇다).
	 *
	 * 프로필이 아니라 여기 두는 이유: 프로필은 서버만 받는다(SetProfile). 클라이언트 복사본은
	 * 프로필이 없으므로 프로필에 넣으면 트레일이 리슨 호스트 화면에만 보인다. 클래스 디폴트는
	 * 모든 머신이 갖고 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UNiagaraSystem> TrailFX;

	/**
	 * 공 자체에 얹히는 이펙트. 뒤로 끌리는 트레일과 달리 메시를 감싸고 같이 날아간다. 비워 두면
	 * 없다. 트레일과 같은 자리(메시 원점)에 붙으므로 둘을 같이 켜도 어긋나지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UNiagaraSystem> BodyFX;

	/** 던진 팀. 초기 복제. */
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Item")
	int32 Team = Teams::None;

	/** 클라이언트 복사본이 첫 넷 업데이트 전에도 같은 방향으로 날도록. 초기 복제. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 InitialVelocity;

	bool bDetonated = false;

private:
	/**
	 * 슬롯이 찼고 이 머신이 그리는 머신이면, 메시에 붙여 팀색을 넣어 띄운다. 그릴 필요가 없으면
	 * 아무 일도 하지 않는다. 트레일과 바디가 같은 처리를 탄다.
	 */
	void SpawnAttachedFX(UNiagaraSystem* FX);

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
