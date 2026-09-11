#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintBurst.generated.h"

class UPaintballProfile;

/** 파열 방향 계산. 풍선과 APaintBurst가 같은 함수를 써서 같은 시드에 같은 그림이 나온다. */
namespace PaintBurst
{
	/**
	 * 시드가 같으면 어느 머신에서나 같다. 방위각은 고르게 돌리고 고도는 [MinPitch, MaxPitch]도
	 * 사이에서 무작위. 위쪽 반구에 고르게 퍼져 바닥과 벽에 두루 닿는다.
	 */
	MINTCHOCO_API void ComputeDirections(int32 Seed, int32 Count, float MinPitchDeg, float MaxPitchDeg, TArray<FVector>& OutDirections);

	/**
	 * 45도로 던진 탄이 RangeCm까지 날아가는 속도(cm/s). 사거리 = v² / (980 × GravityScale)를 뒤집은 것.
	 * 반경을 손으로 속도로 환산하지 않고 그대로 쓰기 위한 함수다.
	 */
	MINTCHOCO_API float SpeedForRange(float RangeCm, float GravityScale);
}

/** APaintBurst 하나가 뿌리는 것. 서버가 정해 초기 복제로 모든 머신에 간다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintBurstParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst")
	TObjectPtr<const UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst")
	uint8 PaintId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst", meta = (ClampMin = "1"))
	int32 Count = 24;

	/**
	 * 탄 속도(cm/s). 반경을 정하는 값이다: 45도로 던진 탄의 사거리 ≈ v² / (980 × 중력 배율).
	 * Standard 탄(중력 0.5)으로 2.5 m는 350, 3 m는 385, 4 m는 445.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float Speed = 385.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst", meta = (ClampMin = "0", ClampMax = "89"))
	float MinPitch = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxPitch = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burst")
	int32 Seed = 0;
};

/**
 * 한 지점에서 페인트탄을 사방으로 뿌리는 일회성 액터. 광역 아이템(꿀풍선, 히어로 랜딩, 꿀벌)이
 * "반경 N m 원형 도포"를 이걸로 한다.
 *
 * 복제 액터라 멀티캐스트가 필요 없다: 서버가 파라미터를 정해 스폰하면 초기 복제 속성이
 * 클라이언트의 BeginPlay 전에 도착하고, 각 머신이 BeginPlay에서 같은 시드로 탄을 날린다.
 * 서버의 탄만 칠하고(스플랫 로그가 그 결과를 나른다) 클라이언트의 탄은 연출이다. 프로필
 * 패키지가 아직 없는 클라이언트는 참조가 늦게 오므로 OnRep에서 한 번 더 시도한다.
 */
UCLASS()
class MINTCHOCO_API APaintBurst : public AActor
{
	GENERATED_BODY()

public:
	APaintBurst();

	/** 서버 전용. 스폰해서 바로 터뜨린다. Paintball이 없으면 아무것도 하지 않는다. */
	static APaintBurst* Spawn(UWorld& World, const FVector& Location, const FPaintBurstParams& InParams, TSubclassOf<APaintBurst> Class = nullptr);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_Params, Category = "Burst")
	FPaintBurstParams Params;

	UFUNCTION()
	void OnRep_Params();

private:
	void Burst();

	bool bBurst = false;
};
