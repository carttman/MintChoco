#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintRain.generated.h"

class UNiagaraSystem;
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

	/**
	 * 스폰하고 첫 행이 떨어지기까지의 시간(초). 그동안 경로에 예고 표식이 순차로 놓인다.
	 * 0이면 예고 없이 곧바로 떨어진다(예전 동작).
	 *
	 * 다른 파라미터와 같은 초기 복제를 타므로 모든 머신이 같은 시점에 시작한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "0", ForceUnits = "s"))
	float LeadInSeconds = 0.0f;

	/**
	 * 리드인 동안 경로에 놓이는 예고 표식. 비어 있으면 표식 없이 기다리기만 한다.
	 *
	 * 행마다 하나씩 가운데 열 자리에, 아래로 트레이스해 지형에 얹는다. 상대도 보고 피해야
	 * 하는 표시라 복제 액터인 여기서 낸다 — 조준 미리보기(본인 화면 전용)와는 다른 것이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain")
	TObjectPtr<UNiagaraSystem> TelegraphFX;

	/**
	 * 표식을 몇 행마다 하나씩 놓을지. 1이면 행마다, 5면 다섯 행에 하나다.
	 *
	 * 표식 수만 줄이고 훑는 시간은 그대로다(간격이 그만큼 늘어난다). 행이 많은 맵에서 표식이
	 * 너무 촘촘해 보일 때 띄운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rain", meta = (ClampMin = "1"))
	int32 TelegraphRowStride = 1;

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

	/**
	 * 리드인 동안 놓이는 예고 표식의 수. Stride 행마다 하나씩이므로 행 수를 Stride로 나눠
	 * 올린다(첫 행에는 언제나 하나 놓인다). Stride가 1 미만이면 1로 친다.
	 */
	static int32 TelegraphCount(int32 RowCount, int32 Stride);

	/**
	 * 예고 표식 사이의 시간(초). 마지막 표식이 놓이는 순간 첫 행이 떨어지도록 리드인을 표식
	 * 수로 나눈다 — 표식을 띄엄띄엄 놓아도 훑기는 리드인 전체에 걸린다. 둘 중 하나가 없으면 0.
	 */
	static float TelegraphInterval(float LeadInSeconds, int32 TelegraphCount);

	/** 액터가 살아 있어야 하는 시간(초). 리드인 + 모든 행 + 마지막 탄이 떨어질 여유. */
	static float Lifespan(const FPaintRainParams& Params);
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

	/** 첫 행을 떨어뜨리고 나머지 행의 반복 타이머를 건다. 리드인이 끝나는 시점에 불린다. */
	void BeginRows();

	void DropRow();

	/** 리드인 동안 행을 하나씩 앞서 훑으며 예고 표식을 놓는다. 데디케이티드 서버는 지나간다. */
	void PlaceTelegraph();

	/** Row(1부터)행 가운데 열 아래의 지면. 못 찾으면 false — 그 행에는 표식을 놓지 않는다. */
	bool FindGroundAtRow(int32 Row, FVector& OutPoint) const;

	FTimerHandle RowTimer;
	int32 NextRow = 1;
	bool bStarted = false;

	FTimerHandle TelegraphTimer;
	int32 NextTelegraphRow = 1;
};
