#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemAimPreview.h"
#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"

#include "HoneyBalloonProfile.generated.h"

class AHoneyBalloonProjectile;
class AUnit;

/**
 * 꿀풍선: 노란 구를 시선 방향으로 던진다. 지형이나 다른 플레이어에 닿으면 터져 페인트탄을
 * 사방으로 뿌리고, 반경 안의 상대를 스턴한다. 즉발(Duration 0).
 *
 * 아이템 키는 던지지 않고 조준을 시작한다. 좌클릭이 던지고 우클릭이 물린다(UGA_HoneyBalloon).
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UHoneyBalloonProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	TSubclassOf<AHoneyBalloonProjectile> ProjectileClass;

	/** 던지는 속도(cm/s). 중력 1이면 45도로 약 15 m 간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "100", ForceUnits = "cm/s"))
	float ThrowSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "0"))
	float GravityScale = 1.0f;

	/** 눈높이에서 시선 방향으로 이만큼 앞에서 태어난다. 자기 캡슐 안에서 터지지 않을 만큼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ThrowOffset = 60.0f;

	/** 조준 중 보여 주는 궤적의 모양. 던지는 본인의 화면에만 그려진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	FItemAimPreviewStyle AimPreview;

	/** 던지는 방향. 눈높이의 컨트롤 회전이고, 서버도 무브에 실려 온 같은 값으로 계산한다. */
	static FVector GetThrowDirection(const AUnit& Unit);

	/** 풍선이 태어나는 자리. 미리보기와 실제 투척이 같은 값을 써야 표시가 맞는다. */
	FVector GetThrowOrigin(const AUnit& Unit, const FVector& Direction) const;

	FVector GetThrowVelocity(const FVector& Direction) const { return Direction * ThrowSpeed; }

	/** 투사체의 구 반지름(cm). 미리보기가 같은 굵기로 훑는다. 클래스가 없으면 0. */
	float GetProjectileRadius() const;

	/** 이 반경 안의 상대가 스턴된다(cm). 페인트가 닿는 범위는 Burst.Speed가 정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunRadius = 300.0f;

	/** 터질 때 뿌리는 탄. PaintId와 Seed는 런타임에 채워진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoneyBalloon")
	FPaintBurstParams Burst;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
