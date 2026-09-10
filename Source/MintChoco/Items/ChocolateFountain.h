#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/TeamTypes.h"

#include "ChocolateFountain.generated.h"

class AUnit;
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
	 * 돔이 통과시키는 유닛인지. 사용자 본인은 팀이 있든 없든 항상 통과하고, 그 밖에는 같은 팀만.
	 * 팀이 없는 세션(로비 없이 켠 PIE)에서는 본인 말고 전부 상대다. 광역 효과의 규칙과 같다.
	 */
	bool IsFriendly(const AUnit* Unit) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "ChocolateFountain")
	int32 GetTeam() const { return Team; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<USphereComponent> Wall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<USphereComponent> Sensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChocolateFountain")
	TObjectPtr<UStaticMeshComponent> Mesh;

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

	/** 서버 전용. 생성 순간 안에 있는 상대를 통과 목록에 넣고 밀어낸다. */
	void AdmitTrappedOpponent(AUnit& Unit);

	/** 이 머신에서 Wall을 무시하게 해 둔 캡슐들. EndPlay에 되돌린다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AUnit>> IgnoringUnits;

	/** 서버 전용. BeginPlay의 초기 오버랩이 끝났는지. 그 뒤에 들어오는 상대는 벽에 막혀야 한다. */
	bool bSpawnOverlapsDone = false;
};
