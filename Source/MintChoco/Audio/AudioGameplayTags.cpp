#include "Audio/AudioGameplayTags.h"

namespace AudioTags
{
	UE_DEFINE_GAMEPLAY_TAG(Audio_Weapon_Fire, "Audio.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Weapon_Empty, "Audio.Weapon.Empty");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Weapon_ChargeLoop, "Audio.Weapon.ChargeLoop");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Weapon_ChargeReady, "Audio.Weapon.ChargeReady");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Weapon_Impact, "Audio.Weapon.Impact");

	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Dash, "Audio.Unit.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Land, "Audio.Unit.Land");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Footstep, "Audio.Unit.Footstep");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Stun_Begin, "Audio.Unit.Stun.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Stun_End, "Audio.Unit.Stun.End");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_SuperArmor_Begin, "Audio.Unit.SuperArmor.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_SuperArmor_End, "Audio.Unit.SuperArmor.End");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Unit_Knockback, "Audio.Unit.Knockback");

	UE_DEFINE_GAMEPLAY_TAG(Audio_Item_Announce, "Audio.Item.Announce");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Item_Pickup, "Audio.Item.Pickup");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Item_Activate, "Audio.Item.Activate");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Item_Expire, "Audio.Item.Expire");

	UE_DEFINE_GAMEPLAY_TAG(Audio_World_Burst, "Audio.World.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Audio_World_Splat, "Audio.World.Splat");
	UE_DEFINE_GAMEPLAY_TAG(Audio_World_Balloon_Hit, "Audio.World.Balloon.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Audio_World_Balloon_Pop, "Audio.World.Balloon.Pop");
	UE_DEFINE_GAMEPLAY_TAG(Audio_World_Balloon_Inflate, "Audio.World.Balloon.Inflate");
	UE_DEFINE_GAMEPLAY_TAG(Audio_World_JumpPad, "Audio.World.JumpPad");

	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_CountdownTick, "Audio.Match.CountdownTick");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_Start, "Audio.Match.Start");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_TimerWarning, "Audio.Match.TimerWarning");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_End_Win, "Audio.Match.End.Win");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_End_Lose, "Audio.Match.End.Lose");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Match_End_Draw, "Audio.Match.End.Draw");

	UE_DEFINE_GAMEPLAY_TAG(Audio_UI_Click, "Audio.UI.Click");
	UE_DEFINE_GAMEPLAY_TAG(Audio_UI_Hover, "Audio.UI.Hover");
	UE_DEFINE_GAMEPLAY_TAG(Audio_UI_Error, "Audio.UI.Error");
	UE_DEFINE_GAMEPLAY_TAG(Audio_UI_RoomFound, "Audio.UI.RoomFound");
	UE_DEFINE_GAMEPLAY_TAG(Audio_UI_Join, "Audio.UI.Join");

	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_Lobby, "Audio.Music.Lobby");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_Waiting, "Audio.Music.Waiting");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_Countdown, "Audio.Music.Countdown");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_Playing, "Audio.Music.Playing");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_FinalRush, "Audio.Music.FinalRush");
	UE_DEFINE_GAMEPLAY_TAG(Audio_Music_Ended, "Audio.Music.Ended");
}
