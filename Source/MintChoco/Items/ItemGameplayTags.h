#pragma once

#include "NativeGameplayTags.h"

/**
 * 아이템 시스템의 네이티브 게임플레이 태그. C++에서 선언하므로 ini 등록이 필요 없고,
 * 오타가 컴파일에서 잡힌다.
 */
namespace ItemTags
{
	/** 아이템 효과 상태의 부모. 어떤 아이템이든 효과 중이면 이 아래의 태그를 하나 가진다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_SweetSpinner);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_SpeedStar);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_InfiniteAmmo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_HeroLanding);

	/**
	 * 상대가 거는 상태. 아이템 태그(State.Item.*)는 "내가 쓴 아이템"이고, 이쪽은 "내가 당한 것"이다.
	 * 스턴 중에는 이동·점프·발사·아이템이 막히고, 슈퍼아머 중에는 스턴과 밀어내기가 무시된다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Status_SuperArmor);

	/** SetByCaller: 아이템 효과의 지속시간(초). 어빌리티가 프로필 값을 GE 스펙에 넣는다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_Duration);
}
