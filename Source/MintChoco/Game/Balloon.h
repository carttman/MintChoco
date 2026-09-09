#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/TeamTypes.h"
#include "Weapons/PaintHitReceiver.h"

#include "Balloon.generated.h"

class UMaterialInstanceDynamic;
class UPaintballProfile;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 풍선의 규칙. UObject가 아니라 테스트가 월드 없이 돌린다.
 *
 * 타격력이 쌓여 체력에 닿으면 터진다. 마지막으로 때린 팀이 터뜨린 팀이다. 페인트 id가
 * 팀이 아니면(예: 아이템 산탄의 예약 id) 피해는 쌓이되 팀은 바뀌지 않는다.
 */
USTRUCT()
struct MINTCHOCO_API FBalloonState
{
	GENERATED_BODY()

	float Damage = 0.0f;
	int32 LastTeam = Teams::None;

	/** 피해를 더한다. 이 타격으로 터졌으면 true. 이미 터진 뒤(Damage >= MaxHealth)의 타격은 무시된다. */
	bool Hit(float HitPower, uint8 PaintId, float MaxHealth);

	/** 0~1. 얼마나 부풀었는지. */
	float GetFraction(float MaxHealth) const { return MaxHealth > 0.0f ? FMath::Clamp(Damage / MaxHealth, 0.0f, 1.0f) : 0.0f; }

	void Reset();
};

/**
 * 맵에 놓는 풍선. 페인트탄이나 스나이퍼 광선에 맞으면 부풀고, 체력만큼 맞으면 터지면서
 * 마지막으로 때린 팀의 페인트탄을 사방으로 뿌린다. 얼마 뒤 같은 자리에 다시 부풀어 오른다.
 *
 * 타격은 FPaintDeposit::ApplyHit이 IPaintHitReceiver로 넘겨준다(서버). 파열은 서버가
 * 시드를 정해 멀티캐스트하고, 서버는 진짜 페인트탄을, 클라이언트는 같은 시드의 연출용
 * 탄을 날린다(무기 컴포넌트의 산탄과 같은 규칙). 부푼 정도와 색은 복제된 상태로 맞춘다.
 *
 * 콜리전은 Paintball 채널만 막는다. 플레이어와 카메라는 통과한다.
 */
UCLASS()
class MINTCHOCO_API ABalloon : public AActor, public IPaintHitReceiver
{
	GENERATED_BODY()

public:
	ABalloon();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ IPaintHitReceiver
	virtual void ReceivePaintHit_Implementation(float HitPower, uint8 PaintId, const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Balloon")
	bool IsPopped() const { return bPopped; }

	UFUNCTION(BlueprintPure, Category = "Balloon")
	float GetDamageFraction() const { return State.GetFraction(MaxHealth); }

	UFUNCTION(BlueprintPure, Category = "Balloon")
	int32 GetLastTeam() const { return State.LastTeam; }

protected:
	/** 터지기까지 견디는 타격력의 합. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon", meta = (ClampMin = "1"))
	float MaxHealth = 100.0f;

	/** 터진 뒤 다시 부풀어 오르기까지(초). 0 이하면 다시 생기지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon", meta = (ClampMin = "0", ForceUnits = "s"))
	float RespawnDelay = 20.0f;

	/** 터질 때 뿌리는 페인트탄. 어떤 탄인지(크기, 중력, 자국)는 이 프로필이 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon|Burst")
	TObjectPtr<UPaintballProfile> BurstPaintball;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon|Burst", meta = (ClampMin = "1"))
	int32 BurstCount = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon|Burst", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float BurstSpeed = 1200.0f;

	/** 다 부풀었을 때의 크기 배율. 1이면 부풀지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon|Look", meta = (ClampMin = "1"))
	float InflateScale = 1.5f;

	/** 메시 머티리얼에서 팀 색을 넣을 벡터 파라미터. 없는 이름이면 색이 바뀌지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balloon|Look")
	FName ColorParameterName = TEXT("Color");

	/** 맞을 때마다, 모든 머신에서. 부푼 비율과 마지막 팀. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Balloon")
	void BP_OnHit(float DamageFraction, int32 LastTeam);

	/** 터지는 순간, 모든 머신에서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Balloon")
	void BP_OnPopped(int32 PoppingTeam);

	/** 다시 부풀어 오르는 순간, 모든 머신에서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Balloon")
	void BP_OnInflated();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Balloon")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Balloon")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	/** 서버 전용. 터뜨리고 탄을 뿌린 뒤 재생성 타이머를 건다. */
	void Pop(int32 PoppingTeam);

	/** 서버 전용. 다시 부풀어 오른다. */
	void Inflate();

	/** 서버는 진짜 탄, 클라이언트는 연출용 탄을 같은 시드로 날린다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBurst(int32 Seed, uint8 PaintId);

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Popped();

	/** 복제된 상태를 크기·색·표시에 반영한다. 서버도 직접 부른다. */
	void ApplyLook();

	/** 팀 색을 쓸 동적 머티리얼. 처음 필요할 때 만든다. */
	UMaterialInstanceDynamic* GetOrCreateMaterial();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FBalloonState State;

	UPROPERTY(ReplicatedUsing = OnRep_Popped)
	bool bPopped = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FVector BaseScale = FVector::OneVector;
	FTimerHandle RespawnTimer;
};
