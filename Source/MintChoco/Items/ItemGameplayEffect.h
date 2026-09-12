#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "ItemGameplayEffect.generated.h"

/**
 * 아이템 효과의 지속시간과 중첩을 맡는 GE. 에셋 없이 생성자에서 구성한다.
 *
 * 지속시간은 SetByCaller(Data.Item.Duration)로 어빌리티가 프로필 값을 넣고, 상태 태그는
 * 스펙의 DynamicGrantedTags로 붙는다. 그래서 이 클래스에는 아이템별 값이 하나도 없다.
 * 그런데도 아이템마다 서브클래스가 하나씩 있는 이유는 스택이 GE 클래스 단위로 묶이기
 * 때문이다: 같은 클래스 하나로 두 아이템을 걸면 스택 한도 1이 서로를 밀어낸다.
 *
 * 같은 아이템을 효과 중에 다시 쓰면 RefreshOnSuccessfulApplication이 타이머만 다시
 * 시작한다. 그것이 "재사용 = 지속시간 갱신" 규칙의 전부다.
 *
 * 상대가 거는 상태(스턴, 슈퍼아머)도 같은 골격을 쓴다. 어빌리티 없이 AUnit이 서버에서 직접
 * 스펙을 만들어 걸며, 태그는 마찬가지로 DynamicGrantedTags다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UItemGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UItemGameplayEffect();
};

UCLASS()
class MINTCHOCO_API UGE_SweetSpinner : public UItemGameplayEffect
{
	GENERATED_BODY()
};

UCLASS()
class MINTCHOCO_API UGE_SpeedStar : public UItemGameplayEffect
{
	GENERATED_BODY()
};

UCLASS()
class MINTCHOCO_API UGE_InfiniteAmmo : public UItemGameplayEffect
{
	GENERATED_BODY()
};

UCLASS()
class MINTCHOCO_API UGE_DessertBombardment : public UItemGameplayEffect
{
	GENERATED_BODY()
};

UCLASS()
class MINTCHOCO_API UGE_HeroLanding : public UItemGameplayEffect
{
	GENERATED_BODY()
};

/** 스턴. AUnit::TryApplyStun이 건다. */
UCLASS()
class MINTCHOCO_API UGE_Stunned : public UItemGameplayEffect
{
	GENERATED_BODY()
};

/** 슈퍼아머. 스턴이 끝나는 순간 AUnit이 이어서 건다. */
UCLASS()
class MINTCHOCO_API UGE_SuperArmor : public UItemGameplayEffect
{
	GENERATED_BODY()
};
