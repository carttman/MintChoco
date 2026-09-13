# 머지 복구 문서 — 조준 모드 10단계

`main` 쪽을 우선해서 머지한 뒤, **이번 10단계에서 만든 것 중 무엇이 사라졌는지** 찾아
되살리기 위한 문서다. 작성 시점 2026-09-13, 브랜치 `QA`, HEAD `d1ab133`.

기능 설명이 아니라 **복구용 목록**이다. 왜 그렇게 만들었는지는 `Docs/SystemDevProgress.md`,
전체 계획은 `Docs/SystemDevPlan.html` 에 있다.

---

## 머지 직후 할 일 (순서대로)

```powershell
powershell -ExecutionPolicy Bypass -File Docs\MergeCheck.ps1
```

76개 항목을 기계적으로 본다. **통과했다고 끝이 아니다** — 이 스크립트는 "통째로 사라진 것"만
잡고 **숫자는 못 본다**. `.uasset` 안의 수치는 아래 [데이터 에셋 값](#데이터-에셋-값-전부) 표를
보고 에디터에서 눈으로 대조해야 한다.

그다음:

```
에디터 종료 → 빌드 → 테스트 42건
```

```
D:\EpicGames\UE_5.8\Engine\Build\BatchFiles\Build.bat MintChocoEditor Win64 Development -project="D:\git\MintChoco\MintChoco.uproject" -waitmutex
```

```
UnrealEditor-Cmd.exe D:\git\MintChoco\MintChoco.uproject -ExecCmds="Automation RunTests MintChoco; Quit" -unattended -nullrhi -abslog=D:\git\MintChoco\Saved\Logs\AutoTest.log
```

기준선은 **42건 전부 통과**다. 머지 전 기준선이 궁금하면 `Result={Fail}` 을 찾으면 된다.

---

## 작업 범위

`4481653`(Merge branch 'main' into QA) **이후** 6개 커밋이 전부다.

| 커밋 | 내용 | 단계 |
|---|---|---|
| `13b620d` | 초콜릿 분수, 샷건 수정 | 8, 4 |
| `5e621fd` | 샷건 수정2 | 0, 1, 샷건 2단 중력 |
| `a077ea7` | 샷건 점유율 수정 | 2, 7 |
| `9048cb8` | 차지샷 애니메이션 및 이펙트 수정 | 9, 5, 발사 자세 |
| `0a33816` | 꿀벌 수정 | 6 |
| `d1ab133` | 히어로 랜딩 개선 | 3 |

커밋 이름이 내용과 잘 안 맞는다(9단계 KO 가 "차지샷 애니메이션" 커밋에 들어 있다). 커밋
제목으로 찾지 말고 이 표를 볼 것.

**개별 파일 되살리기**:

```
git checkout d1ab133 -- <경로>
```

---

## 위험도 — 이 순서로 확인할 것

### A등급 · 조용히 사라지고, 사라진 티도 안 난다

`.uasset` 은 `.gitattributes` 에서 `merge=binary` 라 **충돌이 나면 한쪽이 통째로 이긴다.**
부분 머지가 없다. 아래 셋은 손으로 배선한 것이라 되돌아가도 컴파일은 멀쩡히 되고
**게임을 켜 봐야 알 수 있다.**

1. **`RT_ABP_Unit_V2` 의 AnimGraph 배선** — 아래 [수동 배선](#수동-배선-3건) 참조
2. **`WBP_GameHUD` 에 얹은 `WBP_KnockoutGauge`**
3. **`BP_Unit` 의 `TriggerBlockedTags` 에 있는 `State.Item.Aiming`**

### B등급 · 값만 조용히 되돌아간다

데이터 에셋의 수치. 빌드도 테스트도 통과하는데 감각만 예전으로 돌아간다.
→ [데이터 에셋 값](#데이터-에셋-값-전부) 표 전부 대조.

### C등급 · 없어지면 바로 티가 난다

신규 C++ 파일과 기존 파일에 끼워 넣은 코드. 빌드가 깨지거나 테스트가 실패한다.
→ `MergeCheck.ps1` 이 전부 잡는다.

---

## 신규 파일 (16개)

없어지면 `git checkout d1ab133 -- <경로>` 로 그대로 되살리면 된다. 이 파일들은 이번에
새로 만든 것이라 `main` 과 충돌할 내용이 없다.

| 경로 | 단계 |
|---|---|
| `Source/MintChoco/Items/ItemAimAbility.{h,cpp}` | 0 |
| `Source/MintChoco/Items/AimArcPreview.{h,cpp}` | 2 |
| `Source/MintChoco/Items/LandingMarker.{h,cpp}` | 3 |
| `Source/MintChoco/Game/KnockoutGaugeWidget.{h,cpp}` | 9 |
| `Source/MintChoco/Weapons/PaintVolley.{h,cpp}` | 5 |
| `Source/MintChoco/Tests/KnockoutTest.cpp` | 9 |
| `Content/Assets/UI/Widgets/Game/WBP_KnockoutGauge.uasset` | 9 |
| `Content/Blueprints/Items/BP_BombardmentAimLine.uasset` | 1 |
| `Content/Blueprints/Items/BP_HoneyBalloonAimArc.uasset` | 2 |
| `Content/Blueprints/Weapons/Paintballs/DA_Paintball_SniperVolley.uasset` | 5 |

---

## 단계별 상세

### 8단계 · 초콜릿 분수가 차지샷을 막는다 (`13b620d`)

| 파일 | 무엇을 |
|---|---|
| `Items/ChocolateFountain.h` | `GetPaintId()` 추가 (`BlueprintPure`) |
| `Weapons/PaintSniperProfile.cpp` | `Fire()` 의 트레이스를 **Single → Multi** 로 |

**왜 Multi 인가**(되돌아가면 다시 이 함정에 빠진다): 초콜릿 돔의 벽은 페인트볼 채널을
**Block 이 아니라 Overlap** 한다. 탄은 오버랩 이벤트로 삼켜지는데, `LineTraceSingle` 은
터치를 무시하고 그냥 통과한다. Multi 로 바꿔야 광선이 탄과 같은 벽에서 멈춘다.
벽을 Block 으로 바꾸면 탄이 삼켜지지 않고 튕긴다 — 그 길로 가면 안 된다.

같은 색 돔은 통과시킨다(`Dome->GetPaintId() == Context.PaintId` 면 `continue`).

---

### 4단계 · 무한 탄환 발사 속도 버프 (`13b620d`)

| 파일 | 무엇을 |
|---|---|
| `Weapons/PaintWeaponComponent.h/.cpp` | `FreeShotInterval`(0.1), `FreeShotChargeTime`(0.5), `GetEffectiveShotInterval()`, `GetEffectiveChargeTime()`, `LastShotTime` |
| `Weapons/PaintWeaponProfile.h` | `GetShotInterval()` 에 `case EPaintFireMode::Single` 추가 |

**계획서와 다르다.** 계획서는 버프 값을 `UInfiniteAmmoProfile` 에 노출하라고 했지만 실제로는
**`UPaintWeaponComponent` 의 `EditAnywhere` 프로퍼티**로 들어갔다. 계획서를 믿고 찾으면 없다.

`EPaintFireMode::Single` 은 **원래 연사 간격이 없었다**(클릭하는 만큼 나갔다). 이번에
`LastShotTime` 으로 두 번의 당김 사이 최소 간격을 넣었다. 그래서 `ShotsPerSecond` 가
이제 Single 에서도 의미를 가진다 — 이 줄이 사라지면 샷건이 다시 마우스 속도만큼 나간다.

---

### 0단계 · 조준 모드 토대 (`5e621fd`)

| 파일 | 무엇을 |
|---|---|
| `Items/ItemAimAbility.{h,cpp}` | **신규.** 조준 아이템의 공통 골격 |
| `Items/ItemGameplayTags.{h,cpp}` | `State_Item_Aiming` = `"State.Item.Aiming"` |
| `Items/ItemSlotComponent.h/.cpp` | `IsAiming()`, `ConfirmAim()`, `OnAimConfirmed`, `ServerConfirmAim()` |
| `Game/Unit.cpp` | `StartFire()` 에 라우팅 |
| `Content/Blueprints/Game/BP_Unit.uasset` | **`TriggerBlockedTags` 에 `State.Item.Aiming` 추가** ← A등급 |

흐름:

```
좌클릭 → AUnit::StartFire
        → ItemSlot->IsAiming() 이면 ItemSlot->ConfirmAim() 하고 return
        → 아니면 PaintWeapon->PullTrigger()
```

`BP_Unit` 의 태그가 되돌아가면 **조준 중에 무기가 같이 발사된다.** 컴파일은 멀쩡하다.

---

### 1단계 · 디저트 폭격 조준 모드 (`5e621fd`)

| 파일 | 무엇을 |
|---|---|
| `Items/DessertBombardmentAbility.{h,cpp}` | 부모를 `UItemAbility` → **`UItemAimAbility`** |
| `Items/DessertBombardmentProfile.h` | `AimPreviewClass` 추가 |
| `Items/ItemGameplayEffect.h` | `UGE_DessertBombardment` 클래스 추가 |
| `Content/Blueprints/Items/BP_BombardmentAimLine.uasset` | **신규** 미리보기 액터 |

즉발 → 지속형 전환이다. 전용 GE 클래스는 **스택을 분리하기 위해서만** 있다.

---

### 계획 외 · 샷건 2단 중력 (`5e621fd`) — 7단계 `MaxRange` 를 대체

| 파일 | 무엇을 |
|---|---|
| `Weapons/PaintballProfile.h/.cpp` | `DropAfter`, `DropGravityScale` 추가. `Launch(..., float DropAfterOverride = -1.0f)` |
| `Weapons/PaintProjectile.h/.cpp` | `Init(..., float InDropAfterOverride = -1.0f)`, `ApplyDropGravity()`, `DropTimer` |

**기본값 인자로 넣었다** — `APaintRain`·`APaintBurst`·기존 무기 호출부는 한 줄도 안 고쳤다.
머지가 이 기본값을 지우면 호출부가 전부 깨지므로 빌드에서 바로 잡힌다.

`DropGravityScale` 의 `EditCondition = "DropAfter > 0"` 은 **일부러 뺐다**(5단계가
`DropAfter` 0 에서도 이 값을 쓴다). 되살아나 있으면 다시 지울 것.

---

### 2단계 · 꿀풍선 조준 모드 (`a077ea7`)

| 파일 | 무엇을 |
|---|---|
| `Items/AimArcPreview.{h,cpp}` | **신규.** 점 궤적 + 착탄 표시 범용 액터 |
| `Items/HoneyBalloonAbility.{h,cpp}` | 부모를 **`UItemAimAbility`** 로. `ComputeThrow()` 신설 |
| `Items/HoneyBalloonProfile.h` | `AimPreviewClass` 추가 |
| `Items/ItemProjectile.h/.cpp` | `GetCollisionRadius()` 추가 (CDO 에서 읽는다) |
| `Items/ItemGameplayEffect.h` | `UGE_HoneyBalloon` |
| `Content/Blueprints/Items/BP_HoneyBalloonAimArc.uasset` | **신규** |

`ComputeThrow()` 를 미리보기와 실제 투척이 **함께** 쓴다. 둘이 갈라지면 궤적과 착탄이
어긋나므로, 머지 후 이 함수가 한 곳에서만 불리고 있으면 잘못된 것이다.

`AAimArcPreview` 는 점을 **예측의 시간 간격이 아니라 거리**(`DotSpacing`)로 다시 뽑는다.
시간 간격을 그대로 쓰면 궤적 꼭대기에서 점이 뭉친다.

**함정**: `FPredictProjectilePathParams::OverrideGravityZ` 의 **0 은 "월드 중력을 쓰라"** 는
뜻이다. `RefreshArc` 가 이것을 막고 있다(`IsNearlyZero` 검사). 사라지면 미리보기만 떨어진다.

---

### 7단계 · 샷건 퍼짐 (`a077ea7`)

데이터만 바뀌었다. → [데이터 에셋 값](#데이터-에셋-값-전부)

최대 사거리와 궤적 도포는 **손대지 않았다** — 각각 2단 중력과 기존 `TrailDeposit` 으로
이미 되어 있었다.

**"샷건 1발 = 점유율 1 %" 목표는 보류됐다.** 이 맵(8,660 m²)에서 성립하지 않는다.
1 % = 86.6 m² 인데 한 발이 9.59 m² 다. 맞추려면 맵이 약 900 m² 여야 한다.

---

### 9단계 · KO 판정 (70 % 5초) (`9048cb8`)

| 파일 | 무엇을 |
|---|---|
| `Game/GameGameState.h/.cpp` | `FKnockoutMath`, 복제 2개, 판정 로직, `BlueprintPure` 6개 |
| `Game/GameGameMode.h/.cpp` | `EndMatchByKnockout(int32 Team)` **신규 함수** |
| `Game/KnockoutGaugeWidget.{h,cpp}` | **신규.** 도넛 링 위젯 |
| `Tests/KnockoutTest.cpp` | **신규** |
| `Content/Assets/UI/Widgets/Game/WBP_KnockoutGauge.uasset` | **신규** (빈 껍데기) |
| `Content/Assets/UI/Widgets/Game/WBP_GameHUD.uasset` | **`KnockoutGauge` 배치** ← A등급 |

**기존 코드에 손댄 곳은 덧붙이기 세 군데뿐이다.** 머지가 이것만 지워도 KO 가 안 돈다:

1. `GetLifetimeReplicatedProps` — `KnockoutEndServerTime`, `KnockoutTeam` 복제 2줄
2. `RefreshCoverage()` **끝에 `UpdateKnockout()` 한 줄** ← 가장 놓치기 쉽다
3. `AGameGameMode::EndMatchByKnockout` 신규 함수

판정을 `RefreshCoverage`(0.2초 주기) 끝에서 하는 이유: 타이머를 따로 두면 커버리지와 어긋난다.

`UGameHudWidget` 은 **한 줄도 안 고쳤다.** 위젯은 스스로 GameState 를 찾고 스스로 숨고
나타난다 — `WBP_GameHUD` 에 얹기만 하면 된다. 남이 만든 HUD 로 갈아끼워도 그대로 쓸 수 있다.

**미리보기**: 콘솔에 `mc.KnockoutPreview 0.4` (0 이 방금 시작, 1 이 KO 직전). 음수면 실제 상태.
이 맵에서 70 % 가 거의 안 나오므로 연출 확인에는 사실상 이것뿐이다.

---

### 5단계 · 차지샷 순차 발사 (`9048cb8`)

| 파일 | 무엇을 |
|---|---|
| `Weapons/PaintVolley.{h,cpp}` | **신규.** 총구에서 한 발씩 순차 발사 |
| `Weapons/PaintSniperProfile.h/.cpp` | `Sniper\|Volley` 블록 8개 필드 + `SpawnTrailVolley()` |
| `Content/Blueprints/Weapons/Paintballs/DA_Paintball_SniperVolley.uasset` | **신규** |

`PaintSniperProfile::Fire()` 에서 고친 곳은 **딱 한 군데**다:

```cpp
const bool bVolleying = VolleyPaintball != nullptr;
if (!bVolleying || !bSkipTrailWhenVolleying) { PaintTrail(...); }
if (bVolleying) { SpawnTrailVolley(..., Context.Instigator, Context.PaintId, Context.Seed); }
```

`VolleyPaintball` 이 비어 있으면 동작이 예전 그대로다 — 그래서 이 블록이 사라져도
빌드는 되고 차지샷만 조용히 옛날 방식으로 돌아간다.

**`APaintRain`(디저트 폭격)을 재사용하지 않았다.** 폭격은 하늘에서 격자 위로 떨어지지만
차지샷은 무기라 총구에서 나가야 한다. 비슷해 보인다고 스포너를 돌려 쓰면 안 된다.
`APaintRain` 과 `FPaintRainParams` 는 **손대지 않았다.**

탄은 전부 **조준선 그대로** 나가고, 순서는 "언제 꺾이느냐"로 만든다: n 번째 탄은
`Spacing × n` 만큼 직진한 뒤 흘러내린다. 그 직진 시간을 `Init` 의 `DropAfterOverride` 로 넘긴다.

**알려진 한계**: 총구 앞 약 8 m 이내는 안 칠해진다. 빠른 탄이 1.5 m 를 떨어지려면 거리가
필요해서 물리적으로 피할 수 없다 — 버그가 아니다.

---

### 계획 외 · 발사 자세와 총구 정합 (`9048cb8`) ★ 최고 위험

플레이테스트에서 나온 건이라 계획서에 없다. **원인을 세 번 잘못 짚은 끝에 잡았다.**

| 파일 | 무엇을 |
|---|---|
| `Game/UnitAnimInstance.h/.cpp` | `bIsAiming`, **`bWeaponPoseHeld`** |
| `Game/Unit.h/.cpp` | `HandleChargingChanged()`, `StartChargePose()`, `StopChargePose()`, `ChargePose` |
| `Game/UnitDataAsset.h` | `EUnitAction::Charge` (값 5, **맨 뒤에 추가**) |
| `Weapons/PaintWeaponComponent.h/.cpp` | `OnChargingChanged`, `IsAiming()`, `AimReadyDelay`(0), `AimHoldSeconds`(0.5), `SetAiming()`, `FireWhenAimReady()`, `FireAfterAimReady()`, `GetMuzzleAttachment()`, `ApplyChargingVisuals()`, `bShotPending`, `bCancelAfterPendingShot` |
| `Content/Assets/RT_UnitAnimations/RT_ABP_Unit_V2.uasset` | **수동 배선** ← A등급 |
| `Content/Game/Data/DA_Unit_Mint.uasset`, `DA_Unit_Choco.uasset` | `ActionFeedback` 에 `Charge` 항목 |

**함정: `ABP_Unit` 은 고아다.** `Content/Blueprints/Game/ABP_Unit` 은 아무도 참조하지 않는다.
실제로 도는 것은 **`/Game/Assets/RT_UnitAnimations/RT_ABP_Unit_V2`** 이고,
`DA_Unit_Mint`·`DA_Unit_Choco` 의 `AnimClass` 가 그것을 가리킨다.
**애니메이션 문제를 볼 때는 반드시 `DA_Unit_*` 의 `AnimClass` 를 먼저 읽을 것.**

`bWeaponPoseHeld = bIsAiming || bRecentlyFired` 이고, `bIsAiming` 은 무기의 `IsAiming()` 에서
온다: 방아쇠를 당긴 순간부터, 그리고 충전하는 내내 참이다. 덕분에

- 샷건 첫 발이 자세가 올라온 **뒤에** 나간다(`AimReadyDelay` 만큼 미룬다)
- 차지샷이 충전 내내 자세를 유지한다
- 구경하는 다른 클라이언트에서도 자세가 보인다(`bIsFiring` 은 복제되지 않아 안 보였다)

왜 필요했나: **발사 지점은 쏘는 그 순간의 총구**이고 총은 캐릭터 메시의 `Gun` 소켓에 붙어
있다. 자세가 안 올라간 채로 쏘면 탄이 쉬는 손 위치에서 나간다.

충전 자세는 `UpperBody` 슬롯에 **루프로** 걸고(`StartChargePose`), 놓을 때 **그 몽타주만
지목해** 세운다(`StopChargePose`). 슬롯째 세우면 방금 시작한 발사 동작까지 끊긴다.

`StartChargeFX`·`PlayMuzzleFX` 가 `MuzzleSource`(총 메시)를 무시하고 손 소켓에 붙고 있었다.
`GetMuzzleAttachment()` 로 `ComputeMuzzleTransform` 과 같은 순서로 고르게 했다.

---

### 6단계 · 꿀벌 파열 (`0a33816`)

데이터 **한 값**만 바뀌었다: `DA_Item_Bee.Burst.Speed` 100 → **500**.

```
반경 = v² ÷ (980 × GravityScale) = 100² ÷ 980 = 10.2 cm   ← 전
                                    500² ÷ 980 = 255 cm   ← 후
```

20발이 터진 자리에 그대로 쌓이고 있었다. 방식이 아니라 속도가 문제였다.

계획서는 "5단계에서 만든 코드를 그대로 쓴다" 였지만 **스포너를 바꾸지 않았다.**
5단계가 `APaintVolley`(총구 발사)로 갈아엎어졌고 꿀벌에는 총구도 조준선도 없다.
`APaintBurst` 가 이미 "탄을 뿌려 중력으로 떨어뜨리는" 방식이었다.

---

### 3단계 · 히어로 랜딩 (`d1ab133`)

| 파일 | 무엇을 |
|---|---|
| `Game/UnitMovementComponent.h/.cpp` | `SetWantsHeroDive()`, `WantsHeroDive()`, `GetHeroCharge()`, `bWantsHeroDive`, `HeroCharge`, **`FLAG_Custom_3`**, `bSavedWantsHeroDive`, `SavedHeroCharge` |
| `Game/Unit.cpp` | `StartFire()` 에 4줄 |
| `Items/HeroLandingProfile.h` | `MinChargeScale`(0.5), `ChargeScaleFor()` |
| `Items/HeroLandingAbility.cpp` | 마커에 충전량 전달, 착지 효과에 배율 |
| `Items/LandingMarker.{h,cpp}` | **신규.** 원 2개 |
| `Content/Blueprints/Items/BP_LandingMarker.uasset` | **`AStaticMeshActor` → `ALandingMarker` 리페어런트** |

**좌클릭이 RPC 가 아니라 압축 플래그를 타는 이유** — 되돌아가면 반드시 다시 이 함정에 빠진다:

낙하 판단은 `FSavedMove_Unit` 으로 **리플레이되는** 무브먼트 단계 기계 안에 있다.
`ConfirmAim()` 같은 RPC 로 보내면:

```
클라 T초에 낙하 시작 → 이후 무브들은 Dive 상태로 저장
서버 T+RTT/2 에 수신 → 그 사이를 Hover 로 재생 → 위치 벌어짐 → 보정 → 고무줄
```

스피드 스타가 GAS 속성을 못 쓰는 것과 **같은 함정**이다.
**커스텀 플래그를 네 개 다 썼다**: 0 대시 / 1 속도 부스트 / 2 히어로 랜딩 / 3 조기 낙하.
새 예측 기능을 넣으려면 남는 플래그가 없다.

`ConfirmAim`·`IsAiming()`·`State.Item.Aiming` 은 **하나도 안 건드렸다.** `IsAiming()` 은 태그
하나만 보는데 히어로 랜딩의 태그는 `State.Item.HeroLanding` 이고, 그 태그는
`IsHeroLandingAllowed()` 가 서버 검증에 쓰고 있어 바꾸면 안 된다.

**계획서 1번(`UItemAimAbility` 이관)은 일부러 안 했다.** 그 골격은 "조준 → 확정 → 끝" 이고
`OnItemActivated`·`OnItemEnded` 가 `final` 인데, 히어로 랜딩은 확정해도 안 끝난다(좌클릭은
내리꽂기의 *시작*이고 `Burst` 는 착지할 때 난다). 머지 후 누가 이관하려 들면 말릴 것.

`ALandingMarker` 의 루트가 **스케일 없는 `USceneComponent`** 인 이유: 원 하나를 루트로 삼으면
그 스케일이 다른 원에 곱해져 안쪽 원을 줄일수록 두께까지 눌린다.

---

## 데이터 에셋 값 (전부)

머지 후 **에디터에서 눈으로 대조**할 것. 스크립트는 이 숫자를 못 본다.
표에 없는 필드는 이번에 안 건드렸다.

### 무기 · 탄

| 에셋 | 필드 | 값 |
|---|---|---|
| `DA_Scatter_Fan` | `FanHalfAngleDeg` | **30** (전 25) |
| | `SpreadHalfAngleDeg` | **3** (전 1) |
| | `MuzzleSpeed` / `PelletsPerShot` / `Pattern` | 2400 / 5 / `HorizontalFan` |
| `DA_Scatter_Spinner` | `MuzzleSpeed` / `PelletsPerShot` / `Pattern` | 600 / 1 / `Cone` |
| `DA_Weapon_Fan` | `FireMode` / `ShotsPerSecond` | `Single` / **4** |
| | `ChargeTime` / `MinChargeToFire` / `InkCostPercent` | 3 / 1 / 5 |
| `DA_Weapon_Shotgun` | (고아 에셋 — 아무도 참조 안 함) | `Single` / 4 / 3 / 1 / 4 |
| `DA_Weapon_Sniper` | `FireMode` / `ShotsPerSecond` | `Charged` / 8 |
| | `ChargeTime` / `MinChargeToFire` | 3 / **0.3** |
| | `Range` | **3800** |
| | `TrailSpacing` / `MaxTrailSplats` | 40 / 256 |
| | `VolleyPaintball` | `DA_Paintball_SniperVolley` |
| | `VolleyClass` | `None` |
| | `VolleySpacing` / `VolleyInterval` | **300** / **0.04** |
| | `VolleySpeed` / `VolleyDropLead` | **2500** / **600** |
| | `MaxVolleyShots` / `bSkipTrailWhenVolleying` | **64** / **true** |

> **"샷건" 은 `DA_Weapon_Shotgun` 이 아니다.** `BP_Unit` 의 `PaintWeapon.Profile` 은
> **`DA_Weapon_Fan`** 이다. 무기 값을 볼 때는 Fan 을 봐야 한다.

| 에셋 | 필드 | 값 |
|---|---|---|
| `DA_Paintball_Heavy_Trail` | `Radius` | 12 |
| | `GravityScale` | **0.05** |
| | `DropAfter` / `DropGravityScale` | **0.2** / **4** |
| | `Deposit.BrushProfile` | `DA_Brush_Mop` |
| | `Deposit.SplatVolume` | **2.4** (전 1.6) |
| | `Deposit.HeightAdd` / `HitPower` | 0.6 / 25 |
| | `TrailDeposit.BrushProfile` | `DA_Brush_Paintball` |
| | `TrailDeposit.SplatVolume` | 0.6 |
| `DA_Paintball_SniperVolley` (신규) | `ProjectileClass` | `BP_Paintball_C` |
| | `Radius` | **12** (샷건 탄과 같은 값 — 50 이면 쏘자마자 바닥에 걸린다) |
| | `GravityScale` | 0.05 |
| | `DropAfter` / `DropGravityScale` | **0** / **2** |
| | `Deposit.BrushProfile` | `DA_Brush_Bomb` |
| | `Deposit.SplatVolume` | **1.35** |
| | `Deposit.HitPower` / `StunDuration` | **0** / **0** (타격은 광선이 이미 한다) |

### 아이템

| 에셋 | 필드 | 값 |
|---|---|---|
| `DA_Item_HoneyBalloon` | `Duration` | **10** (조준 제한 시간) |
| | `AbilityClass` | `GA_HoneyBalloon` |
| | `AimPreviewClass` | **`BP_HoneyBalloonAimArc_C`** |
| `DA_Item_DessertBombardment` | `Duration` | **10** |
| | `AbilityClass` | `GA_DessertBombardment` |
| | `AimPreviewClass` | **`BP_BombardmentAimLine_C`** |
| `DA_Item_Bee` | `Burst.Paintball` | `DA_Paintball_Heavy` |
| | `Burst.Count` | 20 |
| | **`Burst.Speed`** | **500** (전 100) |
| | `Burst.MinPitch` / `MaxPitch` | 20 / 70 |
| | `StunRadius` | 250 |
| `DA_Item_HeroLanding` | `Burst.Paintball` | `DA_Paintball_HeroLanding` |
| | **`Burst.Count`** | **8** (전 1) |
| | **`Burst.Speed`** | **310** (전 445) |
| | `Burst.MinPitch` / **`MaxPitch`** | 0 / **30** (전 0) |
| | `Burst.BurstFXScale` | 1.5 |
| | `StunRadius` | 600 |
| | **`MinChargeScale`** | **0.5** (신규 필드) |
| | `AimMarkerClass` | `BP_LandingMarker_C` |
| | `Landing.RiseHeight` / `RiseTime` | 735 / 0.6 |
| | `Landing.HoverTime` | 2 |
| | `Landing.MaxAimDistance` / `DiveSpeed` / `AimTraceDistance` | 1500 / 3000 / 4000 |

> 히어로 랜딩 `Burst` 3값은 **함께** 맞춘 것이다. 하나만 되돌아가도 계산이 깨진다:
> ```
> 스플랫 반경 = 150 × √3 + 0.005 × 310 = 261 cm
> 산탄 사거리 = 310² ÷ (980 × 0.25)    = 392 cm
> 최대 착탄   = 392 × sin(2 × 30°)     = 339 cm
> 칠해지는 반경 = 339 + 261 = 600 cm  ← StunRadius, 마커 바깥 원과 일치
> ```
> `MinPitch` 0 이 중요하다. 20 부터 시작하면(꿀벌의 값) 가장 가까운 착탄이 389 cm 라
> **발밑에 도넛 구멍**이 남는다.

### BP_LandingMarker CDO

| 필드 | 값 |
|---|---|
| 부모 클래스 | **`/Script/MintChoco.LandingMarker`** |
| `RingMesh` | `/Engine/BasicShapes/Cylinder` |
| `RingMaterial` | `/Game/Blueprints/Items/MI_LandingMarker` |
| `RingThickness` | 4 |
| `MinRadiusFraction` | **0.5** (`MinChargeScale` 와 같은 값이어야 표시와 실제가 맞는다) |
| `ChargeRingLift` | 2 |

---

## 수동 배선 3건

`.uasset` 안이라 git 이 못 지켜 주고 스크립트도 **존재 여부만** 본다.
머지 후 **반드시 에디터에서 눈으로 확인**할 것.

### 1. `RT_ABP_Unit_V2` — 상체 자세 스위치 ★

**위치**: `/Game/Assets/RT_UnitAnimations/RT_ABP_Unit_V2` → AnimGraph →
**첫 번째 `Blend Poses by bool`** 노드

**있어야 할 것**: 그 노드의 **`Active Value`** 핀에 **`Weapon Pose Held`** 가 꽂혀 있어야 한다.

**되돌아간 모습**: `Recently Fired` 가 꽂혀 있다.

**증상**: 충전하는 내내 팔이 아래로 내려가 있고, 탄이 쉬는 손 위치에서 나간다.
컴파일도 되고 테스트도 통과한다.

> `Weapon Pose Held` 는 C++ 상속 변수라 **"Show Inherited Variables"** 를 켜야 목록에 보인다.
> 안 보인다고 없는 것이 아니다.

### 2. `WBP_GameHUD` — KO 게이지

**있어야 할 것**: `WBP_KnockoutGauge` 인스턴스가 **`KnockoutGauge`** 라는 이름으로 얹혀 있다
(상단 중앙 점유율 게이지 아래).

**설정할 것은 없다.** 위젯이 스스로 GameState 를 찾고 스스로 숨고 나타난다. 얹기만 하면 된다.

**증상**: KO 조건이 성립해도 도넛 링이 안 뜬다. 경기는 정상적으로 끝난다.

### 3. `BP_Unit` — `TriggerBlockedTags`

**있어야 할 것**: `State.Item.Aiming`, `State.Item.HeroLanding`, `State.Item.SweetSpinner` **3개**.

**증상**: 조준 중에 무기가 같이 발사된다.

---

## 계획서와 달라진 점 (머지 후 혼란 방지)

`Docs/SystemDevPlan.html` 을 그대로 믿으면 안 되는 자리들이다.

| 계획서 | 실제 |
|---|---|
| 7단계가 `DA_Scatter_Spread` / `DA_Paintball_Light` 를 가리킨다 | 실제는 **`DA_Scatter_Fan` / `DA_Paintball_Heavy_Trail`** |
| 4단계 버프 값을 `UInfiniteAmmoProfile` 에 | 실제는 **`UPaintWeaponComponent`** |
| 7단계에 `MaxRange` 추가 | 안 했다. **2단 중력**(`DropAfter`/`DropGravityScale`)이 대신한다 |
| 6단계가 5단계 코드를 그대로 쓴다 | 안 썼다. `APaintBurst` 의 **속도 한 값**만 고쳤다 |
| 3단계 1번 — 마커/틱을 `UItemAimAbility` 로 이관 | **일부러 안 했다** (위 3단계 참조) |
| "샷건 1발 = 점유율 1 %" | **보류.** 이 맵 크기에서 성립하지 않는다 |

---

## 남은 일 (머지와 무관)

- **PIE 검증이 전부 밀려 있다.** 0·1·2·3·9단계는 빌드와 자동화 테스트만 통과했다.
  3단계는 특히 **리슨 서버 + 클라이언트 2인**에서 조기 낙하가 양쪽에 같게 보이는지,
  고무줄이 없는지를 봐야 한다.
- 히어로 랜딩: 탄이 8발이 되면서 한 착지가 나르는 `HitPower` 가 10 → 80 이다.
  풍선(체력 100)이 거의 한 방에 터진다. 의도한 값이 아니라 발수의 부수 효과다.
- 히어로 랜딩: 약한 착지의 도포 반경(약 430 cm)이 마커가 보여 주는 스턴 반경(300 cm)보다
  크다. 스플랫 반경은 `SplatVolume` 이 탄 에셋에 있어 스폰마다 못 바꾼다.
  없애려면 `FPaintBurstParams` 에 도포 배율을 더해야 하는데 꿀풍선·꿀벌이 같이 쓰는
  구조체라 사전 고지가 필요한 변경이다.
