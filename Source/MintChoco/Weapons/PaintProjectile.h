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

/** How long a ball flies before it dies in the air. The crosshair's impact prediction simulates this long too. */
inline constexpr float PaintballLifeSpanSeconds = 5.0f;

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
	/**
	 * InDropAfterOverride 가 0 이상이면 프로필의 DropAfter 대신 그 시간을 쓴다.
	 *
	 * 프로필의 값은 “이 탄은 언제나 이 거리에서 떨어진다” 는 뜻이다. 한 번의 사격이 여러 발을
	 * 서로 다른 거리에 떨어뜨려야 할 때(차지샷의 연속 발사)만 발마다 다른 값을 받는다.
	 *
	 * InVisualOffset 은 메시가 출발하는 곳(궤적 기준 월드 오프셋). 물리는 시선 위의 원점에서
	 * 날고, 메시만 총구에서 시작해 프로필의 VisualMergeSeconds 동안 궤적으로 미끄러져 들어온다.
	 */
	void Init(const UPaintballProfile* InProfile, uint8 InPaintId, int32 InSeed, const FVector& Velocity, bool bInCosmetic,
		float InDropAfterOverride = -1.0f, const FVector& InVisualOffset = FVector::ZeroVector);

	/** 이 공이 칠하는 id(팀). 초콜릿 돔이 상대 탄을 가려낼 때 본다. */
	uint8 GetPaintId() const { return PaintId; }

	/**
	 * 이 공이 닿을 때 착탄음을 낼지. 산탄처럼 한 번에 여러 발이 나가는 무기가 첫 탄 하나만
	 * 남겨 두는 데 쓴다. Init이 매번 참으로 되돌리므로 풀에서 꺼낸 공에 지난 값이 남지 않는다.
	 */
	void SetPlaysImpactSound(bool bPlays) { bPlaysImpactSound = bPlays; }

	/** The body material a ball of this paint id wears, or null when the id has none and keeps the mesh's own. */
	UMaterialInterface* GetTeamMaterial(uint8 InPaintId) const;

	/**
	 * 풀에서 꺼낸 공을 날 수 있는 상태로 되돌린다. 반드시 Init 뒤에 불린다.
	 *
	 * 콜리전을 다시 켜는 순간 엔진이 그 자리의 초기 오버랩을 곧바로 돌린다. 그 앞에
	 * Init이 끝나 있어야 총구에서 닿는 것(쓴 사람의 캡슐, 초코돔)이 이번 사격의
	 * PaintId·Profile과 무시 목록으로 판정된다. 지난 사격의 값으로 판정하면 제 팀 탄에
	 * 쓴 사람이 기절하고, 자기 돔이 제 팀 탄을 삼킨다.
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

	/** 수명이 다했을 때. 파괴 대신 풀로 돌아간다. */
	virtual void LifeSpanExpired() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Ticks while the mesh merges onto the path and while this ball paints a trail; otherwise the movement component alone drives it. */
	virtual void Tick(float DeltaSeconds) override;

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
	/**
	 * Paints the surfaces around one point of the flight path: rays spread around the plane across
	 * the direction of travel, so the same code covers floor, ceiling and walls whichever way the
	 * ball is heading. Returns how many splats it left.
	 */
	int32 PaintTrailSample(const FVector& Location, int32 SampleIndex);

	/** DropAfter가 지났다. 여기서부터 무겁게 떨어진다. */
	void ApplyDropGravity();

	/** 착탄음을 낼 공인지. SetPlaysImpactSound가 끄지 않는 한 참이다. */
	bool bPlaysImpactSound = true;

	UPROPERTY(Transient)
	TObjectPtr<const UPaintballProfile> Profile;

	uint8 PaintId = 0;
	int32 Seed = 0;
	bool bCosmetic = false;

	/** Where the mesh still is relative to the path, in world space; shrinks to zero over the profile's VisualMergeSeconds. */
	FVector VisualOffset = FVector::ZeroVector;
	float MergeElapsed = 0.0f;
	bool bMerging = false;

	/** Only the server's real ball paints a trail, and only when its profile has one. */
	bool bPaintsTrail = false;

	/** Trail bookkeeping. Distance is measured along the real path, so a lobbed arc samples evenly. */
	FVector LastTrailLocation = FVector::ZeroVector;
	float TrailDistance = 0.0f;
	int32 TrailSampleCount = 0;
	int32 TrailSplatCount = 0;

	/**
	 * 중력을 바꾸는 타이머. Tick이 아닌 이유는 Tick이 궤적 도포용이라 서버의 진짜 탄에서만
	 * 켜지기 때문이다. 타이머는 모든 머신에서 같은 시각에 돌아 연출 탄도 함께 떨어진다.
	 */
	FTimerHandle DropTimer;
};
