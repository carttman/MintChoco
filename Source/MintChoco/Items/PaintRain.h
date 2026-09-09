#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintRain.generated.h"

class UPaintballProfile;

/** APaintRain 하나가 뿌리는 것. 서버가 정해 초기 복제로 모든 머신에 간다. */
USTRUCT(BlueprintType)
struct MINTCHOCO_API FPaintRainParams
{
	GENERATED_BODY()

	/** 첫 행의 기준점(사용자 위치). 행은 여기서 Direction으로 RowSpacing씩 나아간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FVector_NetQuantize Origin;

	/** 수평 단위 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	FVector2D Direction = FVector2D(1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0"))
	int32 RowCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "10", ForceUnits = "cm"))
	float RowSpacing = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "1"))
	int32 Columns = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ColumnSpacing = 100.0f;

	/** 탄이 태어나는 높이(월드 Z). 맵 경계 상자의 최고점 + 여유. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	float DropZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float DropSpeed = 1500.0f;

	/** 행 사이의 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float Interval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	TObjectPtr<const UPaintballProfile> Paintball;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	uint8 PaintId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	int32 Seed = 0;
};

/** 폭격 계획. 월드 없이 테스트한다. */
struct MINTCHOCO_API FPaintRainPlan
{
	/**
	 * Origin에서 Direction으로 Spacing씩 나아갈 때 Bounds 안에 있는 마지막 행 번호(1부터). 경계
	 * 밖에서 시작해도 경계 반대편까지 센다. 한 행도 안에 없으면 0. MaxRows가 상한.
	 */
	static int32 CountRows(const FVector2D& Origin, const FVector2D& Direction, const FBox2D& Bounds, float Spacing, int32 MaxRows);

	/** Row(1부터)행 Column열 탄의 시작점. 열은 진행 방향의 오른쪽으로 펼쳐지고 가운데 열이 중심선이다. */
	static FVector RowPoint(const FPaintRainParams& Params, int32 Row, int32 Column);
};

/**
 * 디저트 폭격: 한 줄로 늘어선 지점 위에서 페인트탄이 순서대로 떨어진다. APaintBurst와 같은
 * 복제 스포너 패턴이다. 서버가 계획을 정해 스폰하면 각 머신이 같은 타이머로 행을 떨어뜨리고,
 * 서버의 탄만 칠한다.
 */
UCLASS()
class MINTCHOCO_API APaintRain : public AActor
{
	GENERATED_BODY()

public:
	APaintRain();

	/** 서버 전용. Paintball이 없거나 행이 없으면 아무것도 하지 않는다. */
	static APaintRain* Spawn(UWorld& World, const FPaintRainParams& InParams, TSubclassOf<APaintRain> Class = nullptr);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_Params, Category = "Rain")
	FPaintRainParams Params;

	UFUNCTION()
	void OnRep_Params();

private:
	void Start();
	void DropRow();

	FTimerHandle RowTimer;
	int32 NextRow = 1;
	bool bStarted = false;
};
