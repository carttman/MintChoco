#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/TeamTypes.h"

#include "ChocolateFountain.generated.h"

class AUnit;
class UChocolateFountainProfile;
class UParticleSystem;
class UParticleSystemComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 초콜릿 분수의 돔. 서버가 스폰하는 복제 액터로 Lifetime 뒤에 사라진다.
 *
 * Wall은 폰을 막고 페인트탄과 겹친다. 어느 팀인지는 콜리전 채널로 가릴 수 없으므로 통과는
 * "무시 목록"으로 만든다: 아군 캡슐은 모든 머신에서 Wall을 무시하고(IgnoreComponentWhenMoving,
 * 머신마다 따로이고 복제되지 않는다), 생성 순간 안에 있던 상대는 밖으로 밀리는 동안만 무시
 * 목록(PassThrough, 서버가 정해 복제)에 있다가 Sensor를 벗어나면 빠진다. 액터가 아니라
 * 컴포넌트를 무시해야 같은 액터의 Sensor 오버랩이 살아 있다.
 *
 * 상대 페인트탄은 Wall에 겹치는 순간 사라진다. 클라이언트의 연출 탄도 같은 규칙으로 없앤다.
 * 스나이퍼 광선은 라인트레이스라 Overlap을 지나친다(알려진 한계).
 */
UCLASS()
class MINTCHOCO_API AChocolateFountain : public AActor
{
	GENERATED_BODY()

public:
	AChocolateFountain();

	/** 서버 전용. SpawnActorDeferred와 FinishSpawning 사이에. Instigator(사용자)는 스폰 파라미터로 온다. */
	void Init(int32 InTeam, uint8 InPaintId, float InRadius, float InLifetime);

	/**
	 * 서버 전용. FinishSpawning 뒤에. 발밑 도포를 프로필이 정한 횟수만큼 반복한다. 첫 번째는
	 * 이 자리에서 바로, 나머지는 BurstInterval마다 한 번씩 직전보다 BurstGrowth배 넓게.
	 *
	 * 일정을 능력이 아니라 돔이 들고 있는 이유: 초콜릿 분수는 즉발(Duration 0)이라 능력 인스턴스가
	 * 곧 끝나 타이머를 얹을 자리가 없다. 돔은 정확히 Lifetime 동안 살고 사라질 때 타이머를
	 * 같이 걷으므로, 마지막 도포와 돔의 끝이 저절로 맞는다.
	 */
	void StartGroundBursts(const UChocolateFountainProfile& Profile);

	/**
	 * 돔이 통과시키는 유닛인지. 사용자 본인은 팀이 있든 없든 항상 통과하고, 그 밖에는 같은 팀만.
	 * 팀이 없는 세션(로비 없이 켠 PIE)에서는 본인 말고 전부 상대다. 광역 효과의 규칙과 같다.
	 */
	bool IsFriendly(const AUnit* Unit) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "ChocolateFountain")
	int32 GetTeam() const { return Team; }

	/** 사용자의 페인트 id. 돔이 삼킬 대상인지 가릴 때 밖에서도 본다(히트스캔이 광선을 끊을지 정할 때). */
	UFUNCTION(BlueprintPure, Category = "ChocolateFountain")
	uint8 GetPaintId() const { return PaintId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<USphereComponent> Wall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<USphereComponent> Sensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 돔이 서 있는 동안 끓는 거품. 연출뿐이라 데디케이티드 서버에서는 아예 켜지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain|FX")
	TObjectPtr<UParticleSystemComponent> BubbleFX;

	/** 거품 이펙트. 비어 있으면 거품 없이 돔만 선다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain|FX")
	TObjectPtr<UParticleSystem> BubbleTemplate;

	/**
	 * BubbleReferenceRadius에서의 균일 배율. 실제 배율은 반경에 비례해 늘어나므로 Radius를
	 * 키우면 거품도 따라 커진다. 돔의 Mesh가 반경을 따라가는 것과 같은 규칙이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain|FX", meta = (ClampMin = "0.01"))
	float BubbleScale = 1.0f;

	/**
	 * BubbleScale이 그대로 적용되는 반경. 즉 이펙트가 원래 크기로 덮는 반경이다.
	 * P_ConsGround_Bubble의 고정 바운드가 ±500이라 500을 기본값으로 둔다. 거품이 돔보다
	 * 크거나 작게 보이면 BubbleScale보다 이쪽을 먼저 맞추는 편이 뜻이 분명하다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain|FX", meta = (ClampMin = "1", ForceUnits = "cm"))
	float BubbleReferenceRadius = 500.0f;

	/**
	 * 돔이 사라지기 이 시간 전에 거품이 스폰을 멈춘다. 남아 있던 거품은 제 수명대로 옅어지다
	 * 사라지므로 돔이 꺼지는 순간 뚝 끊기지 않는다. 0이면 끝까지 끓다가 한 번에 사라진다.
	 * Lifetime보다 크면 처음부터 스폰하지 않는 꼴이라 Lifetime으로 잘린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChocolateFountain|FX", meta = (ClampMin = "0", ForceUnits = "s"))
	float BubbleFadeOut = 1.0f;

	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "ChocolateFountain")
	int32 Team = Teams::None;

	/** 사용자의 페인트 id. 팀이 있으면 팀과 같고, 없으면 무기의 id. 상대 탄을 가려낼 때 본다. */
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "ChocolateFountain")
	uint8 PaintId = 0;

	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "ChocolateFountain")
	float Radius = 300.0f;

	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "ChocolateFountain")
	float Lifetime = 5.0f;

	/** 생성 순간 안에 있던 상대. 밖으로 나갈 때까지 Wall을 무시한다. 서버가 정한다. */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_PassThrough, Category = "ChocolateFountain")
	TArray<TObjectPtr<AUnit>> PassThrough;

	UFUNCTION()
	void OnRep_PassThrough();

	/** 돔이 나타날 때, 모든 머신에서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChocolateFountain")
	void BP_OnRaised(int32 OwningTeam);

private:
	UFUNCTION()
	void OnWallBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSensorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSensorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ApplyShape();
	void SetIgnoresWall(AUnit* Unit, bool bIgnore);
	void ApplyPassThrough();

	/** 거품을 켜고, 꺼야 할 시각에 맞춰 타이머를 건다. 그릴 머신에서만. */
	void StartBubbles();

	/**
	 * 이펙트가 제 길이를 다 재생했을 때. Cascade 에셋 자체는 한 번만 도는 데다 EmitterLoops는
	 * 모듈 안이라 코드에서 못 건드리므로, 끝날 때마다 다시 틀어 돔이 서 있는 동안을 채운다.
	 */
	UFUNCTION()
	void OnBubblesFinished(UParticleSystemComponent* FinishedComponent);

	/** 다음 프레임에 다시 튼다. 완료 브로드캐스트 안에서 재활성하지 않으려고 한 프레임 미룬다. */
	void RestartBubbles();

	/** 스폰만 멈춘다. 이미 떠 있는 거품은 제 수명을 마저 살고 사라진다. */
	void BeginBubbleFadeOut();

	FTimerHandle BubbleFadeTimer;

	/** 페이드가 시작된 뒤로는 다시 틀지 않는다. */
	bool bBubblesFading = false;

	/** 서버 전용. 생성 순간 안에 있는 상대를 통과 목록에 넣고 밀어낸다. */
	void AdmitTrappedOpponent(AUnit& Unit);

	/** 서버 전용. 다음 발밑 도포 하나를 뿌리고, 남았으면 다시 예약한다. */
	void FireGroundBurst();

	/**
	 * 발밑 도포에 쓰는 프로필. 서버에만 있고 복제하지 않는다 — 도포를 뿌리는 것은 서버뿐이고,
	 * 각 APaintBurst가 제 파라미터를 스스로 복제해 나른다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<const UChocolateFountainProfile> BurstProfile;

	FTimerHandle GroundBurstTimer;

	/** 지금까지 뿌린 도포 수. BurstProfile->BurstCount에 닿으면 멈춘다. */
	int32 GroundBurstsDone = 0;

	/** 다음 도포에 쓸 반경 배율. 한 번 뿌릴 때마다 BurstGrowth가 곱해진다. */
	float GroundBurstScale = 1.0f;

	/** 첫 도포 이후 흐른 시간. 마지막 도포를 수명 안으로 당길 때 본다. */
	float GroundBurstElapsed = 0.0f;

	/** 이 머신에서 Wall을 무시하게 해 둔 캡슐들. EndPlay에 되돌린다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUnit>> IgnoringUnits;

	/** 서버 전용. BeginPlay의 초기 오버랩이 끝났는지. 그 뒤에 들어오는 상대는 벽에 막혀야 한다. */
	bool bSpawnOverlapsDone = false;
};
