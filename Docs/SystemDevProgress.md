# 조준 모드 개발 — 진행 상황

전체 계획은 `Docs/SystemDevPlan.html`(10단계, 착수 순서와 의존 관계)에 있다.
이 문서는 **지금 어디까지 했고 다음이 무엇인지**만 적는다. 작업이 끝날 때마다 갱신한다.

최종 갱신: 2026-09-13 · 브랜치 QA

## 권장 순서와 현황

`8 → 4 → 0 → 1 → 2 → 7 → 9 → 5 → 6 → 3`

| 단계 | 내용 | 상태 |
|---|---|---|
| 8 | 초콜릿 분수가 차지샷을 막도록 | **완료** |
| 4 | 무한 탄환 발사 속도 버프 + Single 연사 간격 신설 | **완료** |
| 0 | 공통 조준 모드 토대 (`UItemAimAbility`) | **완료** (PIE 미검증) |
| 1 | 디저트 폭격 조준 모드 + 조준선 | **완료** |
| — | 샷건 2단 중력 (계획 외, 7단계 `MaxRange` 를 대체) | **완료** |
| 2 | 꿀풍선 조준 모드 | **완료** (PIE 미검증) |
| 7 | 샷건 퍼짐 · 점유율 1 % | **완료** (점유율 목표는 보류 — 아래 참조) |
| 9 | KO 판정 (70 % 5초) | **완료** (PIE 미검증) |
| 5 | 차지샷 순차 발사 | **완료 · PIE 확인됨** |
| — | 발사 자세·총구 정합 (계획 외, 플레이테스트에서 나옴) | **완료 · PIE 확인됨** |
| 6 | 꿀벌 낙하 방식 전환 | **완료** (PIE 미검증) |
| **3** | **히어로 랜딩 (가장 위험)** | **다음 작업 — 마지막** |

## 확정된 설계 결정

- **조준 제한 시간 10초.** 디저트 폭격 확정, 꿀풍선도 동일. 히어로 랜딩은 기존 `Duration 6`초 유지.
- **조준 취소 입력 없음.** 빠져나가는 길은 10초 만료뿐.
- **만료 시 아이템은 잃는다.** 활성화 시점에 슬롯이 비므로 추가 코드가 없다.
- **차지샷 이중 도포 허용.** 기존 `Trail` 도포를 끄지 않는다. 5단계 이후 P0의 "풀충전 5 %"를 다시 재야 한다.
- **"샷건 1발 = 점유율 1 %" 목표는 보류.** 이 맵에서 성립하지 않는다(아래 계산). 현재 수준을 지키고
  소폭만 올리기로 했다. 맵 크기가 정해지면 다시 계산한다.

## 조준 모드 구조 (0단계 산출물)

```
좌클릭 → AUnit::StartFire
        → ItemSlot->IsAiming() 이면 ItemSlot->ConfirmAim() 하고 return
        → 아니면 PaintWeapon->PullTrigger()
```

- `UItemAimAbility`(`Items/ItemAimAbility.h`)가 미리보기 수명 · 틱 조준 추적 · 확정 수신 · 정리를 전담.
  서브클래스는 `OnAimBegan` / `SpawnPreview` / `UpdatePreview` / `OnAimConfirmed` / `OnAimAborted` 만 채운다.
- 상태 태그 `State.Item.Aiming` (네이티브, `ItemGameplayTags`). `BP_Unit` 의 `TriggerBlockedTags` 에 등록돼 있어
  조준 중에는 무기가 발사되지 않는다.
- `UItemSlotComponent::ConfirmAim()` 이 예측 클라이언트에서 먼저 알리고 서버에 `ServerConfirmAim()` RPC.
  무기의 `ServerFire` 예측과 같은 규칙. 중복 확정은 `bConfirmed` 가 막는다.
- **본보기**: `UGA_DessertBombardment`(1단계)와 `UGA_HoneyBalloon`(2단계). 즉발 → 지속형 전환, 전용 GE 클래스
  (스택 분리용), 미리보기 BP 는 프로필의 `TSubclassOf<...>` 로 뺐다. 셋 다 같은 모양이므로 3단계도 이대로.
- **`AAimArcPreview`**(2단계 산출물, `Items/AimArcPreview.h`)는 점 궤적 + 착탄 표시를 그리는 범용 액터다.
  `UInstancedStaticMeshComponent` 하나에 점을 심고, 개수가 같으면 `BatchUpdateInstancesTransforms` 로
  자리만 고쳐 쓴다. 점은 예측의 시간 간격이 아니라 **거리**(`DotSpacing`) 로 다시 뽑는다 — 시간 간격을
  그대로 쓰면 궤적 꼭대기에서 점이 뭉친다. 던지는 아이템이 더 생기면 이 액터를 그대로 쓰면 된다.

## 미커밋 변경 (HEAD = 5e621fd)

0 · 1 · 4 · 8단계와 샷건 2단 중력은 `5e621fd` 로 커밋됐다. 지금 남은 것은 **2단계뿐**이다.

신규: `Source/MintChoco/Items/AimArcPreview.{h,cpp}`, `Content/Blueprints/Items/BP_HoneyBalloonAimArc.uasset`

수정: `HoneyBalloonAbility.{h,cpp}`, `HoneyBalloonProfile.h`, `ItemGameplayEffect.h`,
`ItemProjectile.{h,cpp}`, `ItemProfileAssetTest.cpp`, `DA_Item_HoneyBalloon`

## 7단계: 점유율 계산 (2026-09-12 실측)

`Lvl_Stage` 도포 가능 면적 = **86,600,505 cm² ≈ 8,660 m²**. 도포 큐브 31개, **전부 `bPaintUp` 만**
켜져 있어 윗면 넓이의 합이 곧 분모다. 바닥 하나(`_61`)가 8000 × 8000 cm 로 전체의 74 %.
플랫폼이 덮은 바닥 셀도 분모에 남으므로 **이론상 점유율 상한은 약 74 %** 다(별개 이슈).

샷건 한 발(최대 사거리 약 11.6 m):

| | 계산 | 면적 |
|---|---|---|
| 착탄 5발 | `DA_Brush_Mop` 40 × √2.4 = 반경 62.0 cm | 6.03 m² |
| 궤적 도포 | 발당 5샘플 × 반경 21.3 cm | 3.56 m² |
| **합계** | | **9.59 m² = 0.111 %** |

1 % = 86.6 m² 이므로 **약 9배 차이**다. `SplatVolume` 을 1.6 → 33 으로 올리면 닿지만, 잉크가
발당 5 % · 초당 15 % 회복(지속 3발/초)이고 매치가 90초라 한 명이 290발을 쏜다 → 30초면 맵이
포화된다. 지금 수치(한 명 최대 약 30 %)가 90초 매치에 맞다. 1 % 가 맞으려면 맵이 약 900 m² 여야 한다.

적용값: `DA_Scatter_Fan` 팬 25°→**30°**, 지터 1°→**3°** / `DA_Paintball_Heavy_Trail` `Deposit.SplatVolume` 1.6→**2.4**.
최대 사거리와 궤적 도포는 손대지 않았다 — 각각 2단 중력과 기존 `TrailDeposit` 으로 **이미 되어 있었다**.

## 9단계: KO 판정 구조

**UI 가 통째로 바뀔 수 있다는 전제로 짰다.** 규칙과 표시를 완전히 갈라 두었다.

- 상태와 규칙은 전부 `AGameGameState` 에 있다. UI 가 알아야 할 것은 `BlueprintPure` 6개
  (`IsKnockoutPending` / `GetKnockoutTeam` / `GetKnockoutRemaining` / `GetKnockoutProgress` /
  `GetKnockoutThreshold` / `GetKnockoutHoldSeconds`)와 이벤트 `BP_OnKnockoutPendingChanged` 뿐이다.
- `UKnockoutGaugeWidget`(`Game/KnockoutGaugeWidget.h`)은 **도넛 링**이다. 링과 안쪽 숫자를
  **전부 NativePaint 에서 직접 그린다** — `UPaintChargeWidget`(ChargeRing)·`UPaintCoverageBarWidget`
  과 같은 방식이고, 그래서 **자식 위젯이 하나도 필요 없다**(`WBP_KnockoutGauge` 는 빈 껍데기다).
  `BuildArc` 가 12시부터 시계 방향으로 호를 그리고, 안쪽에 5·4·3·2·1 이 선다.
- 이 위젯은 **`UGameHudWidget` 을 전혀 모른다.** 스스로 GameState 를 찾고 스스로 숨고 나타난다.
  붙이는 방법 셋: ① HUD 어딘가에 놓기만 한다(설정할 것 없음) ② 위젯 BP 로 상속해
  색·굵기·폰트만 바꾸고 `BP_OnKnockoutChanged` / `BP_OnKnockoutSecond` 로 연출을 얹기
  ③ 이 위젯을 아예 안 쓰고 자기 위젯에서 GameState 를 직접 읽기.
- **미리보기**: 70 %까지 칠하지 않고 모양만 보려면 콘솔에 `mc.KnockoutPreview 0.4`
  (0 이 방금 시작, 1 이 KO 직전). 음수로 되돌리면 실제 상태를 쓴다. 이 맵에서 KO 조건이
  거의 안 나오므로 연출 확인에는 이것이 사실상 유일한 길이다.
- **`UGameHudWidget` 은 한 줄도 고치지 않았다.** HUD 를 다른 사람이 만든 것으로 바꿔도
  `WBP_KnockoutGauge` 를 얹기만 하면 된다.
- 남은 초가 아니라 **서버 시각**(`KnockoutEndServerTime`)을 복제한다 — 경기 타이머와 같은 방식.
- 판정은 `RefreshCoverage`(0.2초 주기) 끝에서 한다. 타이머를 따로 두면 커버리지와 어긋난다.

기존 코드에서 고친 곳은 **덧붙이기 세 군데**뿐이다: `GetLifetimeReplicatedProps`(복제 2줄),
`RefreshCoverage`(끝에 `UpdateKnockout()` 한 줄), `AGameGameMode`(새 함수 `EndMatchByKnockout` 추가).

### 주의: 이 맵에서 70 % 는 거의 불가능하다

플랫폼(두께 있는 블록)이 깔고 앉은 바닥 1,868 m² 는 영영 칠할 수 없는데 분모에는 그대로 남는다.
8,660 m² 중 도달 가능한 면적은 약 6,790 m² = **78 %**. 따라서 70 % 는 도달 가능한 면적의 **89 %** 다.
기준은 `AGameGameState::KnockoutThreshold` 로 노출돼 있으니 데이터로 조정하면 된다(기본 0.7 은 요청값).

## 5단계: 차지샷 순차 발사

**총구에서** 조준선을 따라 한 발씩 차례로 나간다. 좌클릭 무기와 같은 자리, 같은 높이다.

### APaintRain 을 쓰지 않은 이유

처음에 디저트 폭격의 `APaintRain` 을 재사용했다가 되돌렸다. 폭격은 **하늘에서** 격자 위로
떨어지지만 차지샷은 **무기**다 — 같은 순차 연출이라도 탄이 태어나는 곳이 다르면 전혀 다른
무기가 된다. 메커니즘이 비슷해 보인다고 스포너를 돌려 쓰면 안 된다.

그래서 `APaintVolley`(`Weapons/PaintVolley.h`)를 새로 만들었다. `APaintRain` 과
`FPaintRainParams` 는 **손대지 않았다** — 폭격은 그대로다.

### APaintVolley 구조

- `Origin` 은 언제나 **총구**(`Context.Muzzle`). 탄은 거기서 착탄 지점을 향해 날아간다.
- n 번째 탄은 조준선 위 `Spacing × n` 지점 **아래의 바닥**을 향한다. 바닥은 각 머신이 같은
  자리에서 아래로 한 번 트레이스해 찾으므로, **목표 지점 목록을 복제할 필요가 없다**
  (파라미터가 고정 크기로 남는다). 바닥을 못 찾은 지점은 건너뛴다.
- 서버가 스폰하면 초기 복제로 파라미터가 가고, 각 머신이 같은 타이머로 같은 순서로 쏜다.
  서버의 탄만 칠하고 클라이언트의 탄은 그림이다(`APaintRain`·`APaintBurst` 와 같은 규칙).
- 첫 발은 기다리지 않는다. 방아쇠를 당긴 순간 총구에서 무언가 나가야 한다.

### 즉발 Trail 을 끈 이유

`bSkipTrailWhenVolleying` 기본 **true**. 즉발 도포를 함께 켜 두면 발사 즉시 줄무늬가 다
칠해진 뒤에 탄이 도착해 **순차 연출이 눈에 보이지 않는다.** `VolleyPaintball` 이 비어 있으면
이 값은 아무 뜻도 없으므로, 데이터를 넣기 전까지 동작은 그대로였다.

### 사거리

`Range` **3,800 cm(38 m)**. 맵에서 가능한 가장 먼 사격은 바닥의 대각선
8000 × √2 = **11,314 cm(113 m)** 이고, 그 **1/3 = 3,771 cm** 를 반올림한 값이다.
(가장자리 기준으로 재면 8000 / 3 = 2,667 cm 이지만, “끝의 끝” 은 대각선이다.)

### 탄환 크기

`Radius` **12** — 좌클릭 샷건 탄(`DA_Paintball_Heavy_Trail`)과 같은 값이다. 폭격 탄에서
복제해 온 50 을 그대로 두었더니 콜리전 구체가 너무 커서 쏘자마자 바닥에 걸려 사라졌다.
`Radius` 는 콜리전과 메시 크기이고, **칠하는 넓이는 `Deposit` 이 따로 정한다** — 둘은 무관하다.

### 도포량

전용 탄 `DA_Paintball_SniperVolley`: `DA_Brush_Bomb` 브러시에 `SplatVolume` **1.35**,
`GravityScale` **0.05**(조준한 바닥 지점에 그대로 맞도록), `HitPower`·`StunDuration` 0
(타격 판정은 광선이 이미 한다). 스플랫 반경은 `150√1.35 + 0.005 × 6000` = **204 cm**.

| | 사거리 | 발수 | 한 발 점유율 |
|---|---|---|---|
| 기존 Trail | 10,000 cm | 250 샘플 | 약 47 m² = **0.54 %** |
| 순차 발사(사거리 제한 전) | 10,000 cm | 32 발 | 약 420 m² = **4.8 %** |
| **지금** | 3,800 cm | **12 발** | 약 157 m² = **1.8 %** |

사거리를 1/3 로 줄였으므로 줄무늬 길이도 1/3 이고 도포량도 그만큼 줄었다.
P0 의 “풀충전 5 %” 를 이 사거리에서 맞추려면 `VolleySpacing` 을 150 으로 좁혀 24 발로 늘리고
`SplatVolume` 을 1.95 로 올리면 된다(반경 239 cm). **아직 적용하지 않았다** — 밸런스 결정이다.

## 계획 외: 발사 자세와 총구 정합 (2026-09-13)

플레이테스트에서 나온 건이라 계획서에 없다. 원인을 세 번 잘못 짚은 끝에 잡았으므로 남긴다.

### 함정: ABP_Unit 은 고아다

`DA_Unit_Mint` 와 `DA_Unit_Choco` 의 `AnimClass` 는 **`RT_ABP_Unit_V2`**
(`Content/Assets/RT_UnitAnimations/`)다. `Content/Blueprints/Game/ABP_Unit` 은 **아무도 쓰지 않는다.**
애니메이션 문제를 볼 때는 반드시 `DA_Unit_*` 의 `AnimClass` 를 먼저 확인할 것.

`UUnitAnimInstance::bRecentlyFired` 주석이 “상체 에임 오프셋 전환에 쓴다” 고 말하는데, 그것이
사실인 곳은 `RT_ABP_Unit_V2` 다. `ABP_Unit` 에서는 아무 데도 안 꽂혀 있어서 한 번 헛짚었다.

### 구조

상체 자세는 `RT_ABP_Unit_V2` 의 **첫 번째 `Blend Poses by bool`** 이 켜고 끈다.
그 `Active Value` 를 `Recently Fired` → **`Weapon Pose Held`** 로 바꿔 두었다(수동 배선).

`bWeaponPoseHeld = bIsAiming || bRecentlyFired` 이고, `bIsAiming` 은 무기의 `IsAiming()` 에서 온다:
방아쇠를 당긴 순간부터, 그리고 충전하는 내내 참이다. 덕분에

- 샷건 첫 발이 자세가 올라온 **뒤에** 나간다(`AimReadyDelay` 만큼 미룬다)
- 차지샷이 충전 내내 자세를 유지한다
- 구경하는 다른 클라이언트에서도 자세가 보인다(`bIsFiring` 은 복제되지 않아 안 보였다)

왜 필요했나: **발사 지점은 쏘는 그 순간의 총구**이고, 총은 캐릭터 메시의 `Gun` 소켓에 붙어 있다.
자세가 안 올라간 채로 쏘면 탄이 쉬는 손 위치에서 나간다.

충전 자세 자체는 `EUnitAction::Charge` 피드백을 `UpperBody` 슬롯에 **루프로** 걸고(`StartChargePose`),
놓을 때 **그 몽타주만 지목해** 세운다(`StopChargePose`). 슬롯째 세우면 방금 시작한 발사 동작까지 끊긴다.

`StartChargeFX` 와 `PlayMuzzleFX` 는 `MuzzleSource`(총 메시)를 무시하고 손 소켓에 붙고 있었다.
`GetMuzzleAttachment()` 를 만들어 `ComputeMuzzleTransform` 과 같은 순서로 고르게 했다.

### 차지샷 탄이 조준선을 따라간다

`APaintVolley` 가 처음엔 **바닥을 겨누고** 있었다(탄마다 다른 바닥 지점). 총구 높이가 1.5 m 뿐이라
하향각이 3번 탄부터 10° 아래로 떨어져 사실상 수평이었고, 위로 조준하면 궤적선과 완전히 갈라졌다.

지금은 **모든 탄이 조준선 그대로** 나가고, 순서는 “언제 꺾이느냐” 로 만든다: n 번째 탄은
`Spacing × n` 만큼 직진한 뒤 흘러내린다. 발마다 다른 직진 시간은 `APaintProjectile::Init` 의
`DropAfterOverride`(기본값 인자)로 넘긴다 — `APaintRain`·`APaintBurst` 는 그대로다.

`VolleyDropLead`(600 cm)는 **낙하 중에도 앞으로 나아가는 거리**를 미리 빼 주는 값이다.
속도 2500 · `DropGravityScale` 2.0 이면 1.5 m 낙하에 약 9.8 m 를 나아간다.

**알려진 한계**: 총구 앞 약 8 m 이내는 칠해지지 않는다. 빠른 탄이 1.5 m 를 떨어지려면 거리가
필요해서 물리적으로 피할 수 없다. 완만한 낙하를 고른 대가다.

## 6단계: 꿀벌 파열 (2026-09-13)

**스포너를 바꾸지 않았다.** 계획서는 “5단계에서 만든 코드를 그대로 쓴다” 였지만, 5단계가
`APaintRain`(하늘 낙하)에서 `APaintVolley`(총구 발사)로 갈아엎어졌고 꿀벌에는 총구도
조준선도 없다. 그대로 가져다 쓸 수 있는 코드가 없어졌다.

대신 원인을 다시 봤더니 **`APaintBurst` 는 이미 “탄을 뿌려 중력으로 떨어뜨리는” 방식**이었고,
문제는 방식이 아니라 속도였다:

```
반경 = v² ÷ (980 × GravityScale) = 100² ÷ (980 × 1) = 10.2 cm
```

20발이 터진 자리에 그대로 쌓이고 있었다. `Speed` 100 → **500** (반경 약 255 cm, 꿀벌의
`StunRadius` 250 과 맞춘 값). 도포 면적은 20발 × 반경 50.6 cm = 약 16 m² 로, 그 원을 거의 채운다.

**데이터 한 값만 바꿨다**(`DA_Item_Bee.Burst.Speed`). 팀 색 도포는 `OnDetonate` 가 이미
`Burst.PaintId = GetPaintId()` 로 넣고 있어 따로 할 일이 없었다.

참고: 파열 아이템의 탄은 `DropAfter` 를 0 으로 둬야 한다. 위 사거리 공식이 성립하지 않게 된다.

## 확인하지 않은 것

- **0 · 1 · 2단계는 PIE 에서 검증되지 않았다.** 빌드와 자동화 테스트(40/40)만 통과했다.
  조준 → 좌클릭 → 발동, 10초 만료, 표시가 실제 결과와 일치하는지는 아직 눈으로 안 봤다.
- 꿀풍선 궤적은 `UGameplayStatics::PredictProjectilePath` 로 그린다. 발사 지점·속도는
  `UGA_HoneyBalloon::ComputeThrow` 하나에서 나오므로 미리보기와 실제 투척이 어긋날 수 없다.
  다만 **확정 순간의 서버 시선**으로 던지므로, 클릭 직전에 크게 돌리면 RTT 만큼 차이가 날 수 있다.
- 샷건 2단 중력(`DropAfter 0.2` / `DropGravityScale 4` / `GravityScale 0.05`)의 손맛도 미검증.
- 7단계 값(팬 30°, 지터 3°, 볼륨 2.4)은 계산으로만 잡았다. 퍼짐 폭은 취향이라 PIE 에서 봐야 한다.
- 6단계 꿀벌 파열 반경 255 cm 는 계산으로만 잡았다. 꿀풍선(163 cm)보다 넓은데, 꿀벌이
  상대를 쫓아가 터지는 아이템이라 스턴 반경과 맞춘 것이다. 너무 넓으면 `Speed` 를 낮춘다.
- 9단계 KO 는 순수 계산만 테스트(`MintChoco.Match.Knockout`)됐다. 실제로 70 %까지 칠해 본 적은 없다.
  **링을 화면에서 본 적이 없다** — MCP 로는 콘솔 명령을 넣을 수 없어 `mc.KnockoutPreview` 를
  대신 쳐 줄 수가 없었다. 반지름 26 px · 굵기 6 px · 슬롯 84 × 84 는 계산으로만 잡은 값이다.

## 함정 (실제로 시간을 날린 것들)

- **계획서(`SystemDevPlan.html`)의 에셋 이름을 믿지 말 것.** 7단계는 `DA_Scatter_Spread` / `DA_Paintball_Light`
  를 가리키는데 실제로 쓰이는 것은 `DA_Scatter_Fan` / `DA_Paintball_Heavy_Trail` 이다. 늘 `BP_Unit` 에서
  참조를 따라갈 것.
- **"샷건"은 `DA_Weapon_Shotgun` 이 아니다.** `BP_Unit` 의 `PaintWeapon.Profile` 은 `DA_Weapon_Fan` 이고,
  `DA_Weapon_Shotgun` 은 어디에서도 참조되지 않는 고아 에셋이다. 무기 값을 바꿀 때는 Fan 을 봐야 한다.
- `EPaintFireMode::Single` 은 원래 연사 간격이 없었다(클릭하는 만큼 나갔다). 4단계에서 `LastShotTime` 으로
  두 번의 당김 사이 최소 간격을 넣었다. 그래서 `ShotsPerSecond` 가 이제 Single 에서도 의미를 가진다.
- 조준 수렴(`PaintGunProfile::Fire`)은 총구 위치만 보정하고 **중력은 보정하지 않는다.** 탄이 크로스헤어보다
  낙차만큼 아래에 맞는 이유다. 샷건은 직진 구간 중력을 0.05 로 낮춰 이를 피해 갔다.
- `FPredictProjectilePathParams::OverrideGravityZ` 의 **0 은 "월드 중력을 쓰라"** 는 뜻이다.
  중력을 끈 설정(`GravityScale 0`)을 그대로 넘기면 미리보기만 떨어진다. `RefreshArc` 가 막고 있다.
- 머지가 선언과 정의를 갈라놓은 적이 두 번 있다(`Unit.cpp` 정의 3개, `PaintWeaponComponent.h` 선언 9개).
  머지 직후에는 빌드가 되는지부터 확인할 것.

## 작업 루프

에디터가 DLL 을 물고 있어 **빌드하려면 에디터를 닫아야 한다.** 자동화 테스트도 에디터가 떠 있으면
포트 8000 을 뺏겨 MCP 가 죽으므로 닫은 상태에서 돌린다.

```
코드 수정 → 에디터 종료 → 빌드 → (데이터 작업이 있으면) 에디터 실행 → 저장 → 종료 → 테스트
```

- 에디터 실행(MCP 포함):
  `UnrealEditor.exe D:\git\MintChoco\MintChoco.uproject -skipcompile -ExecCmds="ModelContextProtocol.StartServer"`
  포트 8000 LISTEN 이 준비 신호.
- 빌드: `D:\EpicGames\UE_5.8\Engine\Build\BatchFiles\Build.bat MintChocoEditor Win64 Development -project="D:\git\MintChoco\MintChoco.uproject" -waitmutex`
- 테스트: `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests MintChoco; Quit" -unattended -nullrhi -abslog=D:\git\MintChoco\Saved\Logs\AutoTest.log`
  로그에서 `Result={Fail}` 을 찾는다. 현재 기준선은 **40건 전부 통과**다.
