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

	/** SetByCaller: 아이템 효과의 지속시간(초). 어빌리티가 프로필 값을 GE 스펙에 넣는다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_Duration);
}
