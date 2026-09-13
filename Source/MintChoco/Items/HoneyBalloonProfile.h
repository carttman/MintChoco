#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"

#include "HoneyBalloonProfile.generated.h"

class AAimArcPreview;
class AHoneyBalloonProjectile;

/**
 * 꿀풍선: 노란 구를 시선 방향으로 던진다. 지형이나 다른 플레이어에 닿으면 터져 페인트탄을
 * 사방으로 뿌리고, 반경 안의 상대를 스턴한다.
 *
 * 지속형이다: Duration은 효과 시간이 아니라 조준 제한 시간이다. 그 안에 던지지 않으면
 * 효과가 만료되고 아이템은 그대로 잃는다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UHoneyBalloonProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	TSubclassOf<AHoneyBalloonProjectile> ProjectileClass;

	/**
	 * 조준 중 포물선 궤적과 착탄 지점을 보여 주는 미리보기. 조준하는 본인 화면에만 생기며,
	 * 실제 투척과 같은 속도·중력·굵기로 예측한다. 비워 두면 표시 없이 조준만 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	TSubclassOf<AAimArcPreview> AimPreviewClass;

	/** 던지는 속도(cm/s). 중력 1이면 45도로 약 15 m 간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "100", ForceUnits = "cm/s"))
	float ThrowSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "0"))
	float GravityScale = 1.0f;

	/** 이 반경 안의 상대가 스턴된다(cm). 페인트가 닿는 범위는 Burst.Speed가 정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunRadius = 300.0f;

	/** 터질 때 뿌리는 탄. PaintId와 Seed는 런타임에 채워진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	FPaintBurstParams Burst;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
