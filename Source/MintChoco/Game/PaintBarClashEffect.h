#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "Math/RandomStream.h"
#include "UObject/Object.h"

#include "PaintBarClashEffect.generated.h"

class FSlateWindowElementList;
struct FGeometry;

/** 격돌 연출이 한 프레임에 받는 값. 좌표와 길이는 바 위젯의 로컬 Slate 단위다. */
struct MINTCHOCO_API FPaintBarClashFrame
{
	/** 두 액체가 맞닿는 점. y는 액체 높이의 가운데다. */
	FVector2f Contact = FVector2f::ZeroVector;

	/** 통 안쪽, 액체가 차는 영역의 높이. */
	float LiquidHeight = 0.0f;

	/** 0..1. 두 액체가 만나면 빠르게 오르고, 떨어지거나 KO가 나면 서서히 내려간다. */
	float Strength = 0.0f;

	/** 맞닿는 점의 가로 속도(단위/초). 양수면 왼쪽 팀이 오른쪽으로 밀어붙이는 중이다. */
	float ContactVelocity = 0.0f;

	FLinearColor LeftColor = FLinearColor::White;
	FLinearColor RightColor = FLinearColor::White;
};

/** 격돌 연출의 입자 하나. */
struct FPaintBarParticle
{
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Velocity = FVector2f::ZeroVector;
	float Age = 0.0f;
	float Lifetime = 1.0f;
	float Size = 1.0f;

	/** 깜빡임 위상과 크기 변화에 쓰는 난수. */
	float Seed = 0.0f;

	/** 방울 연출에서 왼쪽 팀 색인지. */
	bool bLeftTeam = true;
};

/**
 * 두 게이지가 만났을 때의 연출. 바 위젯의 ClashEffects 배열에 인스턴스로 담기고, 디테일 패널에서 종류를 고른다.
 * 위젯이 매 프레임 Tick과 Paint를 부른다. 경계 자체를 흔드는 연출은 GetBoundaryTurbulence와 GetFoamWidth로
 * 바 재질에 값을 넘긴다. 여러 개를 담으면 전부 돌고, 재질 값은 가장 큰 것을 쓴다.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class MINTCHOCO_API UPaintBarClashEffect : public UObject
{
	GENERATED_BODY()

public:
	/** 두 액체가 새로 맞닿은 순간. */
	virtual void OnClashBegin(const FPaintBarClashFrame& Frame) {}

	/** 격돌 중이 아니어도 매 프레임 불린다. Strength가 0이면 새 입자는 만들지 말고 남은 입자만 식힌다. */
	virtual void Tick(const FPaintBarClashFrame& Frame, float DeltaTime) {}

	/** 바 위에 그린다. 사용한 가장 높은 레이어를 돌려준다. */
	virtual int32 Paint(const FPaintBarClashFrame& Frame, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const { return LayerId; }

	/** 바 재질의 경계 요동. 0..1. */
	virtual float GetBoundaryTurbulence() const { return 0.0f; }

	/** 격돌 지점 거품띠의 폭(단위). 0이면 그리지 않는다. */
	virtual float GetFoamWidth() const { return 0.0f; }

	/** 남은 입자를 버린다. */
	virtual void Reset() {}
};

/** 오버워치 느낌의 불꽃. 맞닿는 순간 크게 터지고, 격돌이 이어지는 동안 계속 튄다. */
UCLASS(meta = (DisplayName = "Spark"))
class MINTCHOCO_API UPaintBarSparkEffect : public UPaintBarClashEffect
{
	GENERATED_BODY()

public:
	virtual void OnClashBegin(const FPaintBarClashFrame& Frame) override;
	virtual void Tick(const FPaintBarClashFrame& Frame, float DeltaTime) override;
	virtual int32 Paint(const FPaintBarClashFrame& Frame, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
	virtual void Reset() override;

protected:
	/** 맞닿는 순간 한꺼번에 튀는 불꽃 수. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0", ClampMax = "200"))
	int32 BurstCount = 22;

	/** 격돌이 이어지는 동안 초당 튀는 불꽃 수. Strength 1 기준. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0", ClampMax = "400"))
	float EmitRate = 45.0f;

	/** 맞닿는 점이 움직일 때 속도 1당 늘어나는 초당 불꽃 수. 세게 밀어붙일수록 많이 튄다. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0"))
	float EmitPerContactSpeed = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Spark")
	FFloatInterval Speed = FFloatInterval(140.0f, 360.0f);

	UPROPERTY(EditAnywhere, Category = "Spark")
	FFloatInterval Lifetime = FFloatInterval(0.18f, 0.5f);

	/** 아래로 끄는 가속(단위/초²). */
	UPROPERTY(EditAnywhere, Category = "Spark")
	float Gravity = 620.0f;

	/** 불꽃 꼬리 길이. 속도에 곱하는 초다. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0", ForceUnits = "s"))
	float StreakSeconds = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0.5"))
	float Thickness = 2.2f;

	/** 가로보다 세로로 튀게 하는 정도. 0이면 사방으로, 1이면 거의 위아래로만 튄다. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0", ClampMax = "1"))
	float VerticalBias = 0.45f;

	/** 밀리는 쪽으로 더 튀게 하는 정도. */
	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "0", ClampMax = "1"))
	float PushBias = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Spark", meta = (ClampMin = "1", ClampMax = "1024"))
	int32 MaxSparks = 256;

	/** 갓 튄 불꽃의 색. 수명이 지나며 Hot을 거쳐 Cool로 식는다. */
	UPROPERTY(EditAnywhere, Category = "Spark|Color")
	FLinearColor CoreColor = FLinearColor(FColor(255, 254, 237));

	UPROPERTY(EditAnywhere, Category = "Spark|Color")
	FLinearColor HotColor = FLinearColor(FColor(254, 255, 102));

	UPROPERTY(EditAnywhere, Category = "Spark|Color")
	FLinearColor CoolColor = FLinearColor(FColor(248, 108, 58));

	/** 맞닿는 점의 빛무리 반경. 액체 높이에 대한 배수다. */
	UPROPERTY(EditAnywhere, Category = "Spark|Glow", meta = (ClampMin = "0"))
	float GlowRadius = 1.3f;

	UPROPERTY(EditAnywhere, Category = "Spark|Glow", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.55f;

	/** 빛무리가 일렁이는 속도(초당 회). */
	UPROPERTY(EditAnywhere, Category = "Spark|Glow", meta = (ClampMin = "0"))
	float GlowFlicker = 9.0f;

private:
	void Emit(const FPaintBarClashFrame& Frame, float SpeedScale);

	TArray<FPaintBarParticle> Sparks;
	FRandomStream Random = FRandomStream(0x5BA2C);
	float EmitCarry = 0.0f;
	float Time = 0.0f;
};

/** 성난 파도. 경계가 크게 요동치며 거품이 일고, 두 팀 색 방울이 상대 쪽으로 넘어간다. 파고가 주기적으로 몰아친다. */
UCLASS(meta = (DisplayName = "Surge"))
class MINTCHOCO_API UPaintBarSurgeEffect : public UPaintBarClashEffect
{
	GENERATED_BODY()

public:
	virtual void OnClashBegin(const FPaintBarClashFrame& Frame) override;
	virtual void Tick(const FPaintBarClashFrame& Frame, float DeltaTime) override;
	virtual int32 Paint(const FPaintBarClashFrame& Frame, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;
	virtual float GetBoundaryTurbulence() const override;
	virtual float GetFoamWidth() const override;
	virtual void Reset() override;

protected:
	/** 바 재질에 넘기는 경계 요동. 파고가 가장 높을 때 값이다. */
	UPROPERTY(EditAnywhere, Category = "Surge", meta = (ClampMin = "0", ClampMax = "1"))
	float Turbulence = 0.8f;

	/** 파고가 솟았다 가라앉는 한 주기. 요동과 방울이 이 주기로 몰아친다. */
	UPROPERTY(EditAnywhere, Category = "Surge", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float SurgePeriod = 0.75f;

	/** 가라앉았을 때 남는 세기. 0..1. */
	UPROPERTY(EditAnywhere, Category = "Surge", meta = (ClampMin = "0", ClampMax = "1"))
	float SurgeFloor = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Surge", meta = (ClampMin = "0"))
	float FoamWidth = 2.5f;

	/** 맞닿는 순간 한꺼번에 튀는 방울 수. */
	UPROPERTY(EditAnywhere, Category = "Surge|Droplet", meta = (ClampMin = "0", ClampMax = "200"))
	int32 BurstCount = 14;

	/** 초당 튀는 방울 수. 파고가 가장 높을 때 값이다. */
	UPROPERTY(EditAnywhere, Category = "Surge|Droplet", meta = (ClampMin = "0", ClampMax = "200"))
	float EmitRate = 16.0f;

	UPROPERTY(EditAnywhere, Category = "Surge|Droplet")
	FFloatInterval Speed = FFloatInterval(80.0f, 210.0f);

	UPROPERTY(EditAnywhere, Category = "Surge|Droplet")
	FFloatInterval Lifetime = FFloatInterval(0.45f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "Surge|Droplet")
	FFloatInterval DropletRadius = FFloatInterval(2.5f, 6.0f);

	UPROPERTY(EditAnywhere, Category = "Surge|Droplet")
	float Gravity = 640.0f;

	/** 공기 저항. 걸쭉한 방울이 멀리 날아가지 않게 한다. */
	UPROPERTY(EditAnywhere, Category = "Surge|Droplet", meta = (ClampMin = "0"))
	float Drag = 0.8f;

	/** 방울 위쪽 하이라이트의 밝기. */
	UPROPERTY(EditAnywhere, Category = "Surge|Droplet", meta = (ClampMin = "0", ClampMax = "1"))
	float HighlightStrength = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Surge|Droplet", meta = (ClampMin = "1", ClampMax = "1024"))
	int32 MaxDroplets = 160;

private:
	void Emit(const FPaintBarClashFrame& Frame, float SpeedScale);

	TArray<FPaintBarParticle> Droplets;
	FRandomStream Random = FRandomStream(0x3A71F);
	float EmitCarry = 0.0f;
	float Time = 0.0f;

	/** 지금 파고. Strength까지 곱한 값이다. */
	float SurgeLevel = 0.0f;
};
