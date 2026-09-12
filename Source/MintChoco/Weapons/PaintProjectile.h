#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintProjectile.generated.h"

class UMaterialInterface;
class UPaintballProfile;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Object channel "Paintball" from DefaultEngine.ini: what a ball is, so that balls can be told to ignore each other. */
inline constexpr ECollisionChannel PaintballChannel = ECC_GameTraceChannel1;

/**
 * A paintball in flight. It carries the profile that launched it and paints with its real impact
 * velocity, which is the one thing a hitscan has to fake. The visual mesh and the team body
 * materials are set on the Blueprint; radius and gravity come from the profile.
 *
 * The body material reads three Custom Primitive Data slots written at launch: 0 = launch speed
 * (cm/s), 1 = wobble phase in [0, 1) derived from the seed, 2 = birth time (world seconds). The
 * mesh's local +X is the flight direction, since the rotation follows the velocity.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API APaintProjectile : public AActor
{
	GENERATED_BODY()

public:
	APaintProjectile();

	/**
	 * Call between SpawnActorDeferred and FinishSpawning: the movement component reads the velocity
	 * when it initializes. A cosmetic ball is a client's picture of one the server owns: it flies
	 * and dies identically but leaves no paint.
	 */
	void Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity, bool bInCosmetic);

	/** 이 공이 칠하는 id(팀). 초콜릿 돔이 상대 탄을 가려낼 때 본다. */
	uint8 GetPaintId() const { return PaintId; }

	/** The body material a ball of this paint id wears, or null when the id has none and keeps the mesh's own. */
	UMaterialInterface* GetTeamMaterial(uint8 InPaintId) const;

	/**
	 * 풀에서 꺼낸 공을 날 수 있는 상태로 되돌린다. Init 직전에 불린다.
	 *
	 * 충돌한 무브먼트 컴포넌트는 StopSimulating으로 UpdatedComponent를 null로 만든다.
	 * 속도만 다시 넣으면 공이 제자리에 서 있으므로, 붙잡을 컴포넌트를 다시 알려 줘야 한다.
	 */
	void RestoreForReuse();

	/**
	 * 날기를 멈추고 재운다. 풀 반납과 EndPlay 양쪽에서 불린다.
	 *
	 * 쏜 사람의 콜리전에 박아 둔 상호 무시 항목을 여기서 지운다. 풀로 반납할 때는 EndPlay가
	 * 불리지 않으므로, 이 정리가 여기 있지 않으면 슈터의 무시 목록이 쏠 때마다 하나씩
	 * 영원히 자란다.
	 */
	void Deactivate();

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** 수명이 다했을 때. 파괴 대신 풀로 돌아간다. */
	virtual void LifeSpanExpired() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	/** Pawns are overlapped rather than blocked so a ball never pushes a player; the contact is handled like a hit. */
	UFUNCTION()
	void OnPawnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** One body material per paint id, in team order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint")
	TArray<TObjectPtr<UMaterialInterface>> TeamMaterials;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UPaintballProfile> Profile;

	uint8 PaintId = 0;
	int32 Seed = 0;
	bool bCosmetic = false;
};
