#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"

#include "PaintVolley.generated.h"

class APawn;
class UPaintballProfile;

/** APaintVolley 하나가 쏘는 것. 서버가 정해 초기 복제로 모든 머신에 간다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintVolleyParams
{
	GENERATED_BODY()

	/** 탄이 태어나는 곳. 무기의 총구다 — 하늘이 아니라 총에서 나간다는 것이 이 액터의 요점이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley")
	FVector_NetQuantize Origin;

	/** 조준선의 방향(단위 벡터). 착탄 지점은 이 선 아래의 바닥에서 찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley")
	FVector_NetQuantizeNormal Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** 착탄 지점 사이의 거리(cm). 조준선을 따라 잰다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley", meta = (ClampMin = "10", ForceUnits = "cm"))
	float Spacing = 300.0f;

	/** 한 발과 다음 발 사이의 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float Interval = 0.04f;

	/** 총구를 떠나는 속도(cm/s). 모든 탄이 같은 속도라 한 줄기로 흐른다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float Speed = 6000.0f;

	/**
	 * 목표 지점보다 이만큼 **앞에서** 떨어지기 시작한다(cm).
	 *
	 * 탄은 떨어지는 동안에도 앞으로 나아간다. 목표 바로 위에서 꺾으면 그만큼 지나쳐서
	 * 착탄하므로, 미리 꺾어 두어야 조준한 자리에 떨어진다. 0 이면 보정하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DropLead = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley")
	TObjectPtr<const UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley")
	uint8 PaintId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volley")
	int32 Seed = 0;
};

/**
 * 총구에서 조준선을 따라 페인트탄을 한 발씩 차례로 쏘는 일회성 액터. 차지샷의 "순차 낙하"가
 * 이것이다.
 *
 * APaintRain(디저트 폭격)과는 다른 물건이다. 폭격은 **하늘에서** 격자 위로 떨어지지만, 무기는
 * **총구에서** 나가야 한다: 같은 순차 연출이라도 탄이 태어나는 곳이 다르면 전혀 다른 무기가
 * 된다. 그래서 이 액터의 Origin 은 언제나 총구이고, 탄은 거기서 착탄 지점을 향해 날아간다.
 *
 * n 번째 탄은 조준선 위 Spacing × n 지점 **아래의 바닥**을 향해 날아간다. 바닥은 각 머신이
 * 같은 자리에서 같은 지형을 재서 찾으므로(아래로 한 번 트레이스) 목표 지점 목록을 복제할
 * 필요가 없다. 바닥을 못 찾은 지점은 그냥 건너뛴다 — 허공에 쏠 이유가 없다.
 *
 * 복제 액터라 멀티캐스트가 필요 없다. 서버가 파라미터를 정해 스폰하면 초기 복제 속성이
 * 클라이언트의 BeginPlay 전에 도착하고, 각 머신이 같은 타이머로 같은 순서로 쏜다. 서버의
 * 탄만 칠하고 클라이언트의 탄은 같은 자리의 그림이다.
 */
UCLASS()
class MINTCHOCO_API APaintVolley : public AActor
{
	GENERATED_BODY()

public:
	APaintVolley();

	/** 서버 전용. Paintball 이 없거나 쏠 것이 없으면 아무것도 하지 않는다. */
	static APaintVolley* Spawn(UWorld& World, const FPaintVolleyParams& InParams, APawn* Shooter,
		TSubclassOf<APaintVolley> Class = nullptr);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_Params, Category = "Volley")
	FPaintVolleyParams Params;

	UFUNCTION()
	void OnRep_Params();

private:
	void Start();
	void FireNext();

	FTimerHandle ShotTimer;
	int32 NextIndex = 0;
	bool bStarted = false;
};
