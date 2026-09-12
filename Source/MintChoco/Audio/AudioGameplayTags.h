#pragma once

#include "NativeGameplayTags.h"

/**
 * 사운드 이벤트의 이름표. 코드는 이 태그로만 소리를 부르고, 어떤 파일이 나갈지는
 * 사운드 뱅크(USoundBank)가 정한다. 네이티브 태그라 오타가 컴파일에서 잡히고
 * ini 등록이 필요 없다.
 *
 * 태그를 새로 만들면 뱅크에도 항목을 채워야 한다. 비어 있으면 SoundBankTest가 잡는다.
 */
namespace AudioTags
{
	//~ 무기

	/** 한 발 나갈 때. 무기별로 다른 소리는 프로필의 오버라이드 뱅크가 정한다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Weapon_Fire);

	/** 잉크가 모자라 방아쇠가 거절될 때. 쏘려던 본인에게만 들린다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Weapon_Empty);

	/** 차지형 무기를 당기고 있는 동안의 루프. 놓거나 취소하면 멈춘다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Weapon_ChargeLoop);

	/** 차지가 가득 차 발사 가능해진 순간. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Weapon_ChargeReady);

	/** 페인트볼이 무언가에 닿을 때. 모든 머신이 각자의 공으로 울린다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Weapon_Impact);

	//~ 유닛

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Dash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Land);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Footstep);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Stun_Begin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Stun_End);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_SuperArmor_Begin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_SuperArmor_End);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Unit_Knockback);

	//~ 아이템

	/** 곧 나타난다고 알리는 단계(레이저). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Item_Announce);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Item_Pickup);

	/** 아이템을 쓰는 순간. 아이템별 소리는 프로필의 오버라이드 뱅크가 정한다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Item_Activate);

	/** 지속형 아이템의 효과가 끝나는 순간. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Item_Expire);

	//~ 월드

	/** 페인트가 터지는 모든 경우(풍선, 히어로 랜딩, 허니벌룬, 벌, 초코돔)의 공통 소리. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_Burst);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_Splat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_Balloon_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_Balloon_Pop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_Balloon_Inflate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_World_JumpPad);

	//~ 매치 진행

	/** 카운트다운 초읽기 한 번(3, 2, 1 각각). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_CountdownTick);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_Start);

	/** 남은 시간이 경고선 아래로 내려가는 순간 한 번. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_TimerWarning);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_End_Win);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_End_Lose);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Match_End_Draw);

	//~ UI. 전부 2D로 재생된다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_UI_Click);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_UI_Hover);

	/** 거절당한 조작(팀을 고르지 않고 준비, 방 이름 없이 생성 등). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_UI_Error);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_UI_RoomFound);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_UI_Join);

	//~ 음악. 매치 단계와 짝지어지는 표는 UGameAudioSettings::MusicByPhase에 있다.

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_Lobby);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_Waiting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_Countdown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_Playing);

	/** 막판. 타이머 경고선을 넘어가면 Playing에서 이걸로 갈아탄다. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_FinalRush);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Audio_Music_Ended);
}
