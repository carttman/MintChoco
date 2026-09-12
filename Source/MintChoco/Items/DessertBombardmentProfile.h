#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"

#include "DessertBombardmentProfile.generated.h"

class AActor;
class APaintRain;
class UPaintballProfile;

/**
 * 디저트 폭격: 바라보는 수평 방향으로 맵 끝까지, 가까운 곳부터 순서대로 페인트탄이 떨어진다.
 * 맵의 끝은 도색 가능 표면들의 경계 상자다. 즉발(Duration 0); APaintRain이 이어받는다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UDessertBombardmentProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TObjectPtr<UPaintballProfile> Paintball;

	/**
	 * 조준 중 발사 경로를 보여주는 미리보기. 조준하는 본인 화면에만 생기며,
	 * 매 프레임 사용자 위치와 수평 시선 방향을 따라간다. 비워 두면 표시 없이 조준만 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TSubclassOf<AActor> AimPreviewClass;

	/** 비워 두면 APaintRain 그대로. 연출을 붙이려면 서브클래스 BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TSubclassOf<APaintRain> RainClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "10", ForceUnits = "cm"))
	float RowSpacing = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "1"))
	int32 Columns = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ColumnSpacing = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float RowInterval = 0.05f;

	/** 경계 상자의 최고점에서 이만큼 위에서 떨어진다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DropHeight = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float DropSpeed = 1500.0f;

	/** 행 수의 상한. 맵이 아무리 커도 이 이상은 떨어지지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "1"))
	int32 MaxRows = 200;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
