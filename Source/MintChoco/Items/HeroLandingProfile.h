#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Game/UnitMovementComponent.h"
#include "Items/ItemProfile.h"
#include "Weapons/PaintBurst.h"

#include "HeroLandingProfile.generated.h"

/**
 * 히어로 랜딩: 점프대 높이만큼 뜨고, 공중에서 착지점을 고른 뒤 내리꽂힌다. 착지 지점에서 탄을
 * 뿌리고 반경 안의 상대를 밀어내며 스턴한다. Duration은 안전 상한(보통 6초)이고 착지하면 그
 * 전에 끝난다. 움직임 자체는 UUnitMovementComponent의 단계 기계가 맡는다(Landing).
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UHeroLandingProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	/** 상승·정지·내리꽂기의 수치. 어빌리티가 시작할 때 양쪽 무브먼트에 같은 값을 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	FHeroLandingParams Landing;

	/** 착지 지점에서 이 반경 안의 상대가 밀리고 스턴된다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StunRadius = 400.0f;

	/** 착지 때 뿌리는 탄. PaintId와 Seed는 런타임에 채워진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	FPaintBurstParams Burst;

	/** 소유 클라이언트에만 보이는 착지점 표시. 매 틱 조준점으로 옮겨지고 내리꽂기부터 고정된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HeroLanding")
	TSubclassOf<AActor> AimMarkerClass;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
