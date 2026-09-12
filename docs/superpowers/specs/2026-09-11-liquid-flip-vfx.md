# Liquid Flipbook 이펙트 5종 연결 스펙

2026-09-11, 게임 디자이너 요청.

## 목표

`Content/Liquid_Flipbook_VFX` 팩의 나이아가라 시스템 5개를 게임의 실제 순간에 붙인다. 다섯 개 모두
텍스처와 색감은 꿀풍선 아이템 이펙트(`NS_HoneyBalloonBurst`)와 동일하게 맞춘다.

## 요구사항

| 원본 에셋 | 재생 시점 | 위치 | 크기 |
|---|---|---|---|
| `NS_Liquid_Flip_12` | 차지샷 충전을 시작하는 순간부터, 발사되는 순간까지 (루프) | 총구 | 원본 |
| `NS_Liquid_Flip_4` | 차지샷을 발사하는 순간 1회 | 총구 | 충전량에 비례: 풀충전 1.5배, 최소 충전량 0.5배 |
| `NS_Liquid_Flip_2` | 샷건을 발사하는 순간 1회 | 총구 | 원본 |
| `NS_Liquid_Flip_8` | 히어로 랜딩 아이템으로 위로 올라가기 시작하는 순간 | 발 밑 | 원본의 1.5배 |
| `NS_Liquid_Flip_9` | 히어로 랜딩 착지 순간 | 착지 지점 | 원본의 1.5배 |

## 결정 사항 (디자이너 확답)

1. **충전 이펙트 가시성**: 모두. 적·아군도 상대의 충전을 볼 수 있어야 한다. 충전 상태를 복제한다.
2. **에셋 위치**: 팩 원본은 건드리지 않고 `/Game/Assets/Paint/Niagara`로 복제해 쓴다.
3. **최소 충전**: `DA_Weapon_Sniper.MinChargeToFire`를 1 → 0.3으로 내린다. 충전 30%부터 발사되고,
   크기가 0.5~1.5배 사이에서 실제로 변한다. 스턴 시간이 충전량에 비례하는 기존 로직도 함께 살아난다.

## 조사로 확정된 사실

- 차지샷 = `DA_Weapon_Sniper` (`UPaintSniperProfile`, FireMode Charged, ChargeTime 3).
  샷건 = `DA_Weapon_Shotgun` (`UPaintGunProfile`, FireMode Single, 산탄 패턴은 별도의 `UPaintScatterProfile` 에셋).
- 꿀풍선 이펙트 = `NS_HoneyBalloonBurst`: 메시 렌더러 2개가 팩의 `SM_Tube_02`를 쓰고,
  `OverrideMaterials`로 `MI_HoneyBalloonBurst_A`(`T_liquid_01x3`, 플립북 3×5),
  `MI_HoneyBalloonBurst_B`(`T_liquid_01`, 3×5)를 덮는다. 두 MI의 부모는 팩의 `M_Sprite`다.
  색은 시스템의 `User.TintColor` 기본값 (R 1, G 0.85, B 0, A 1).
- 대상 5종의 현재 재질: Flip_12는 스프라이트 렌더러 2개(`MI_Sprite_Inst_5` = `T_F_liq02` 3×5,
  `MI_Sprite_Inst` = `T_liqEXP` 3×4), Flip_4는 스프라이트 1개(`MI_Sprite_Inst_4` = `T_F_liq05` 3×5),
  Flip_2는 스프라이트 1개(`MI_Sprite_Inst` = `T_liqEXP` 3×4), Flip_8은 메시 1개
  (`OverrideMaterials` = `MI_Sprite_Inst_7`), Flip_9는 메시 2개(`MI_Sprite_Inst_7`, `MI_Sprite_Inst_8`).
- `UPaintWeaponComponent::OnFired`는 이미 **머신당 정확히 한 번** 발생한다(소유자의 예측, 서버의
  실사격, 그 외 머신의 샷 멀티캐스트). 총구 이펙트는 같은 세 지점에 붙이면 중복도 누락도 없다.
- 히어로 랜딩 착지는 서버가 `APaintBurst`를 스폰하고, 그 액터가 초기 복제로 모든 머신에서
  `FPaintBurstParams::BurstFX`를 1회 재생한다. 상승은 상태 태그 `State.Item.HeroLanding`이 붙는
  순간 `UItemSlotComponent::StartEffectFeedback`이 `UItemProfile::ActivateFX`를 캐릭터 메시에
  부착 재생한다(태그가 복제되므로 모든 머신에서 발생).
- 두 경로 모두 **스케일 인자가 없다**. 1.5배를 데이터로 주려면 각 구조체에 배율 필드가 필요하다.

## 범위 밖

- 팀 색상 연동(페인트 id에 따라 이펙트 색을 바꾸는 것). 지금은 꿀풍선과 같은 고정 색이다.
- 사운드, 애니메이션.
- 팩의 `Demo` 맵과 나머지 `NS_Liquid_Flip_*` 에셋.
