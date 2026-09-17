#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"

#include "Weapons/PaintProjectile.h"

#include "TestPaintProjectile.generated.h"

/**
 * 자동화 테스트가 스폰하는 구체 페인트볼. APaintProjectile은 Abstract이라 직접 스폰할 수
 * 없고, 게임의 BP 페인트볼은 메시와 팀 재질을 들고 있어 테스트가 에셋에 의존하게 된다.
 * 여기는 아무것도 더하지 않는다 — 풀의 재사용 동작만 보면 되기 때문이다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API ATestPaintProjectile : public APaintProjectile
{
	GENERATED_BODY()
};

/**
 * 붙어 있는 액터의 콜리전이 켜지거나 꺼지는 순간의 페인트 id를 적어 두는 탐침.
 *
 * AActor::SetActorEnableCollision은 모든 컴포넌트에 OnActorEnableCollisionChanged를 돌린 뒤
 * 그 자리의 초기 오버랩을 곧바로 질의한다. 그러니 이 훅이 불리는 순간이 곧 "총구에서 닿는
 * 것이 무엇으로 판정되는가"가 정해지는 순간이고, 그때 공은 이미 이번 사격의 공이어야 한다.
 *
 * 오버랩 자체를 보지 않는 이유는 자동화 월드에 물리가 돌지 않아 질의가 늘 빈손이기 때문이다.
 * 판정에 쓰이는 값만 보면 물리 없이도 순서를 못 박을 수 있다.
 */
UCLASS(NotBlueprintable, HideDropdown)
class MINTCHOCO_API UTestCollisionProbeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 콜리전이 바뀔 때마다 그 순간의 소유 공 PaintId. */
	TArray<uint8> PaintIdWhenCollisionChanged;

	/** 같은 순간의 이동 무시 목록 크기. 쏜 사람이 거기 없으면 공은 제 주인과 겹친다. */
	TArray<int32> MoveIgnoreCountWhenCollisionChanged;

	virtual void OnActorEnableCollisionChanged() override
	{
		if (const APaintProjectile* const Ball = Cast<APaintProjectile>(GetOwner()))
		{
			PaintIdWhenCollisionChanged.Add(Ball->GetPaintId());
			const UPrimitiveComponent* const Body = Cast<UPrimitiveComponent>(Ball->GetRootComponent());
			MoveIgnoreCountWhenCollisionChanged.Add(Body ? Body->GetMoveIgnoreActors().Num() : -1);
		}
		Super::OnActorEnableCollisionChanged();
	}
};
