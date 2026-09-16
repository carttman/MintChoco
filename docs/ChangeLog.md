# 변경 기록

무엇을 바꿨고, 그것이 **기존 프로그램의 어디에 붙었고**, **무엇을 대체했는지** 남기는 문서다.
`docs/Traps.md`가 "증상 → 원인"을 다룬다면 이쪽은 "무엇이 언제 어떻게 들어왔는지"를 다룬다.

## 이 문서를 쓰는 규칙

- 항목은 **시스템별**로 묶는다. 날짜순이 아니다.
- 같은 항목이 나중에 또 바뀌면 **옛 내용을 지우고 최신 상태만 남긴다.** 이 문서는 변경의
  연혁이 아니라 **현재 무엇이 어떻게 연결되어 있는지**를 말하는 문서다. 연혁이 필요하면
  `git log`를 본다.
- 한 항목은 네 가지를 답한다: **무엇을 / 어디에 붙였나 / 무엇을 대체했나 / 지금 값**.
- 기본값이 꺼짐인 새 스위치는 그렇게 적는다. 기존 동작이 안 바뀐다는 것이 중요한 정보다.
- 커밋은 맨 아래에 한 줄씩만 적는다. 자세한 것은 위의 시스템별 항목에 있다.

---

## 무기 — 페인트 도포

### 발밑 도포 (`FeetDeposit`)

한 발이 나갈 때마다 사수 발밑을 칠한다.

- **어디에**: `UPaintWeaponProfile`에 `FPaintDeposit FeetDeposit` + `float FeetTraceDown`을 더했고,
  `UPaintWeaponComponent::FireOnce()`의 권한 경로에서 `PaintUnderOwner()`를 부른다.
  샷건(`UPaintGunProfile`)과 차지샷(`UPaintSniperProfile`)이 서로 다른 클래스라 공통 부모에 뒀다.
- **방식**: 폰 위치에서 아래로 광선 하나 → 맞은 바닥에 `FPaintDeposit::ApplyHit`.
  **날아가는 탄을 쓰지 않는다.** 액터도 메시도 복제도 없으므로 구조상 화면에 보일 수 없다.
  입사 속도를 0으로 넘겨 `BuildSplat`이 표면 법선을 입사 방향으로 삼게 했다 → `Stretch`가 1이라
  자국이 늘어나지 않고 둥글게 남는다. 다른 폰 위에 서 있으면 건너뛴다.
- **대체한 것**: 없다. 이런 경로가 아예 없었다.
- **기본값**: 브러시를 비우면 `CanPaint()`가 거짓이라 아무 일도 안 한다. 기존 프로필 전부 무영향.
- **지금 값**: `DA_Weapon_Fan` / `DA_Weapon_Sniper` 양쪽에 `DA_Brush_Feet_T`(반경 150cm, 완전 원),
  `splatVolume` 1, `hitPower` 0, `stunDuration` 0, `FeetTraceDown` 300cm.

### 궤적 아래 광선 고정 (`bTrailFirstRayDown`)

투사체 궤적이 바닥을 일관되게 칠하도록 한다.

- **어디에**: `UPaintballProfile`에 `bool bTrailFirstRayDown`, `APaintProjectile::PaintTrailSample()`에서
  0번 광선을 부채꼴에서 빼내 `-FVector::UpVector`로 고정.
- **왜**: 궤적 광선은 비행 방향에 수직인 평면을 **샘플마다 무작위로 돌려** 쓴다. 광선을 늘려도
  어느 것이 바닥을 향할지는 운이고, 바닥에 맞은 자국도 경로에서 좌우로 제각각 떨어진다.
  그래서 "칠해지다 말다" 했다. 나머지 광선은 그대로 부채꼴을 돌아 벽·천장을 맡는다.
- **대체한 것**: 없다. 부채꼴 자체는 그대로다.
- **기본값**: 꺼짐. 꿀벌·스피드스타 등 다른 탄은 전과 같다.
- **지금 값**: `DA_Paintball_Heavy_Trail_T`만 켜짐.

### 펠릿 계단 낙하 (`bStaggerPelletDrop`)

산탄 펠릿이 조준선을 따라 서로 다른 거리에 떨어진다. 착탄 자국이 먼 곳에 몰리지 않는다.

- **어디에**: `UPaintGunProfile`에 스위치와 세 값, `ComputePelletDropAfter()`가 계산한 값을
  기존 `UPaintballProfile::Launch(..., DropAfterOverride, ...)` 인자로 넘긴다.
  **차지샷 볼리(`APaintVolley::FireNext`)가 쓰는 것과 같은 경로다.** 새 낙하 로직은 없다.
- **모양**: 부채꼴 가운데가 가장 멀리, 양끝이 가장 가까이 → 바닥에 삼각형으로 퍼진다.
  좌우 위치는 펠릿 인덱스에서 뽑는다(`Lateral = |2i/(N-1) - 1|`, 가운데 0 양끝 1).
  순서대로 늘어놓으면 왼쪽 가까이에서 오른쪽 멀리로 비스듬한 줄이 되어 부채꼴처럼 안 보인다.
- **대체한 것**: 없다. 끄면 `-1`을 넘겨 프로필의 `DropAfter`를 그대로 쓴다.
- **기본값**: 꺼짐. `DA_Weapon_Shotgun`, `DA_Weapon_Auto` 등 다른 총은 전과 같다.
- **지금 값**: `DA_Weapon_Fan` 양쪽에 켜짐. `Near` 700cm, `Far` 1800cm, `Lead` 660cm.

`Lead`가 660인 이유는 아래 [낙하 중 전진 거리](#낙하-중-전진-거리)를 볼 것.

### 실험용 사본 — `Content/Blueprints/Weapons/_Test/`

페인트 수치를 만지면서 원본을 건드리지 않기 위한 사본 9개다. 브러시 3종이 다른 무기·아이템과
공유되고 있어 원본 수정이 불가능했다.

```
DA_Weapon_Fan_T            ← BP_Unit.PaintWeapon.Profile 이 가리킨다
  Paintball = DA_Paintball_Heavy_Trail_T
                Deposit.brushProfile      = DA_Brush_Mop_T
                TrailDeposit.brushProfile = DA_Brush_Trail_Fan_T
  Scatter   = DA_Scatter_Fan (원본 공유. Fan 전용이라 위험 없음)

DA_Weapon_Sniper_T         ← BP_Unit.SecondaryWeapon.Profile 이 가리킨다
  Impact/Trail.brushProfile = DA_Brush_Paintball_T
  VolleyPaintball           = DA_Paintball_SniperVolley_T
                                Deposit.brushProfile = DA_Brush_Bomb_T

DA_Brush_Feet_T            ← DA_Brush_HeroLanding 복제. 두 무기의 FeetDeposit 이 쓴다
```

`BP_Unit`의 교체는 딱 두 줄이라 되돌리기 쉽다. `Lvl_Stage`에 배치된 `BP_Unit` 인스턴스는
0개이고 `DefaultPawnClass`로 스폰되므로 CDO만 고치면 된다 — 인스턴스 오버라이드 함정은 없다.

### 지금 칠해지는 크기

`반경 = min(BaseRadius × √SplatVolume + RadiusPerSpeed × 속도, MaxRadius)`
가로 반폭이 `Radius`, 진행 방향 반길이가 `Radius × Stretch`이고 `Stretch = clamp(1/cos(입사각), 1, MaxStretch)`다.
**가로만 늘리는 파라미터는 없다.** 가로를 상대적으로 키우려면 `BaseRadius`를 올리고 `MaxStretch`를 내린다.

| 무기 | 브러시 | volume | 가로 반폭 | 비고 |
|---|---|---|---|---|
| 샷건 착탄 | `DA_Brush_Mop_T` (BaseRadius 60, MaxStretch 2.67) | 2.4 | 93cm | 세로는 248cm로 변경 전과 같다 |
| 샷건 궤적 | `DA_Brush_Trail_Fan_T` (BaseRadius 30) | 1.2 | 45cm | 간격 40cm이라 겹쳐서 이어진다 |
| 차지 착탄 | `DA_Brush_Paintball_T` (BaseRadius 12) | 2.0 | 32cm | |
| 차지 궤적 | 같음 | 0.5 | 24cm | 볼리가 켜져 있으면 안 찍힌다 |
| 볼리 착탄 | `DA_Brush_Bomb_T` (BaseRadius 150) | 1.35 | 187cm | |
| 발밑 | `DA_Brush_Feet_T` (BaseRadius 150, MaxStretch 1) | 1.0 | 150cm | 완전 원 |

`MinAlignedStretch`가 1.5다. `Stretch`가 이보다 낮으면 스탬프가 진행 방향 정렬을 버리고
시드로 무작위 회전한다. `MaxStretch`를 1.5 아래로 내리면 방향성이 사라진다.

### 차지샷 볼리 간격과 발수

| | |
|---|---|
| 어디에 | `DA_Weapon_Sniper_T` |
| 무엇을 | `VolleySpacing` 225 → **170cm**, `MaxVolleyShots` 16 → **14** |
| 대체한 것 | 간격 100 → 225 → 170으로 두 번 바뀌었다. 지금 값이 170이다 |

발수는 `clamp(floor((길이 − 간격×0.5) / 간격), 0, MaxVolleyShots)`이라 사거리 2500에서
14발이 나온다. 상한 14는 그 자연수와 같게 맞춰 둔 것이다 — 사거리가 늘면 상한이 먼저 걸린다.

간격 170을 고른 계산:

- 자국 반경 = `DA_Brush_Bomb_T` BaseRadius 150 × √(splatVolume 1.35) = **175cm**
- 줄무늬에서 가장 가는 지점(허리) = `√(반경² − (간격/2)²)`

| 간격 | 발수 | 허리 | 덧칠 |
|---|---|---|---|
| 120 | 20 | 164cm (94%) | 2.9배 |
| **170** | **14** | **153cm (87%)** | **2.1배** |
| 210 | 11 | 144cm (82%) | 1.7배 |

170이 줄무늬 폭을 눈에 띄게 줄이지 않으면서 덧칠만 30% 덜어낸다. 210부터는 가장자리가
물결친다.

`bSkipTrailWhenVolleying`은 **true 그대로 뒀다**. 아래 [연출 보호 장치](#bskiptrailwhenvolleying-는-버그가-아니다) 참고.

### 차지샷 — 충전이 잉크·사거리·스턴을 정한다

충전을 세 군데에 연결했다. 세 가지 모두 **만충이 기준값이고 덜 충전하면 깎인다.**

| | |
|---|---|
| 어디에 | `UPaintWeaponProfile`, `UPaintSniperProfile`, `FPaintDeposit` (C++) + `DA_Weapon_Sniper_T` |
| 대체한 것 | 충전이 공짜였고, 사거리가 충전량과 무관했고, 스턴만 충전비율로 깎였다 |

**충전 시간** `ChargeTime` 3 → **1.5초**, `MinChargeToFire` 0.3 → **0.333**
(= 0.5초. 그 아래로는 발사 자체가 안 된다).

**충전 중 잉크** — `UPaintWeaponProfile`에 새 값 두 개:

| 값 | 지금 | 뜻 |
|---|---|---|
| `ChargeInkPercentPerTick` | **10** | 눈금마다 비우는 잉크(탱크 전체의 %) |
| `ChargeInkTickSeconds` | **0.5** | 눈금 간격. 첫 눈금은 누르고 0.5초 뒤 |

0이 기본이라 이 값을 넣지 않은 프로필은 전과 똑같다. 눈금은 `ChargeTime` 안에 들어가는
수(1.5 ÷ 0.5 = **3회**)까지만 돌아, 만충 한 발이 **30%**다. 계속 쥐고 있어도 더 안 빠진다.
발사 자체의 `InkCostPercent`는 4 → **0**으로 내렸다 — 안 그러면 34%가 된다.

충전 중에 잉크가 바닥나면 **방아쇠를 쥔 머신이 그 자리에서 쏜다**(`OnChargeInkTick` →
`ReleaseTrigger`). 안 그러면 탱크가 빈 쪽이 공짜로 만충까지 갈 수 있어 오히려 이득이 된다.

잉크를 깎는 곳은 **방아쇠를 쥔 머신(예측)과 서버(진짜) 둘뿐이다.** `SetCharging`과
`ServerSetCharging`에서 각각 타이머를 건다. 구경만 하는 머신이 타는 `OnRep_Charging` →
`ApplyChargingVisuals`에는 **넣으면 안 된다** — 같은 잉크를 두 번 깎는다.

**충전량 → 사거리** — `UPaintSniperProfile.RangeHalvingSeconds` **0.5초**.

```
사거리 = Range × 2^(−모자란 초 / RangeHalvingSeconds),  모자란 초 = ChargeTime × (1 − 충전비율)
```

| 충전 | 사거리 |
|---|---|
| 1.5초(만충) | 2500cm |
| 1.0초 | 1250cm (1/2) |
| 0.5초 | 625cm (1/4) |

0이면 충전량이 사거리를 안 바꾸므로 기존 프로필은 전과 같다. 줄어드는 것은 광선의 길이뿐이라
**볼리 발수도 같이 준다** — 덜 충전한 샷은 짧은 줄무늬를 남긴다.

**충전량 → 스턴** — 만충만 두 배로 뛴다.

| 값 | 지금 | |
|---|---|---|
| `Impact.StunDuration` | **1.5** | `ChargeTime`과 같게 둔다. 그래야 부분 충전에서 "충전한 초 = 스턴 초" |
| `FullChargeStunSeconds` | **3.0** | 만충일 때만 이 초를 그대로 쓴다 |
| `FullChargeThreshold` | **0.99** | 만충으로 볼 충전량 |
| `Impact.StunSuperArmorDuration` | **3** | 충전량과 무관하게 고정 |

부분 구간은 코드가 이미 `StunDuration × 충전비율`이라 손댈 것이 없었다. 어긋나는 것은 만충
한 점뿐이므로 거기서만 다른 초를 넘긴다 — `FPaintDeposit::StrikeUnitFor(Hit, PaintId, 초)`가
새로 생긴 함수다. 기존 `StrikeUnit`은 그것을 `StunSecondsFor(Charge)`로 부르는 껍데기가 됐다.

**`FullChargeThreshold`를 1.0으로 두면 안 된다.** 충전량이 네트워크로 갈 때
`FPaintShot::Charge` uint8(0~255)로 눌린다. 한 프레임 차이로 254가 되면 스턴이 3초에서
1.49초로 뚝 떨어진다. 0.99면 252 이상이 전부 만충이다.

슈퍼아머는 **9번 값 하나로 끝났다.** 코드가 슈퍼아머를 충전량으로 깎은 적이 없기 때문이다
(`StunSecondsFor`는 스턴만 곱한다). 자세한 동작은 아래
[슈퍼아머](#슈퍼아머는-스턴이-끝나는-알림에-물려-있다) 참고.

### 샷건 궤적이 들쭉날쭉했던 이유 = 높이

| | |
|---|---|
| 어디에 | `DA_Paintball_Heavy_Trail_T`, `DA_Paintball_Fan_Painter_T` (**양쪽 다**) |
| 무엇을 | `TrailRadius` 250 → **700cm** |
| 대체한 것 | 250cm. 탄이 그보다 높으면 아래 광선이 바닥에 안 닿았다 |

궤적 도포는 비행 경로에서 `TrailRadius`만큼 광선을 뻗어 표면을 찾는다. 탄이 나가는 높이가
발밑 기준 **약 204cm**라, 수평 사격은 250cm 안에 겨우 들어오고 **위를 보고 쏘거나 점프·보드·
단차**에서는 전부 빗나갔다. "어떨 때는 칠해지고 어떨 때는 처음 5발만 칠해진다"가 이것이다.

204cm의 근거: 캡슐 반높이 99 + `CameraBoom.TargetOffset.Z` 45 + `SocketOffset.Z` 60.

`bTrailFirstRayDown`이 이미 한 줄기를 항상 아래로 보내고 있었지만, 그 줄기의 **길이**가
모자랐던 것이라 따로 고쳐야 했다.

### 샷건 연사 (`DA_Weapon_Fan_T.FireMode`)

| | |
|---|---|
| 어디에 | **`DA_Weapon_Fan_T`** (`_Test` 사본). `BP_Unit.PaintWeapon.Profile`이 가리키는 그 애셋 |
| 무엇을 | `FireMode` `Single` → **`Automatic`**. 방아쇠를 누르고 있으면 계속 나간다 |
| 대체한 것 | 한 번 누를 때 한 발 |
| 값 | `ShotsPerSecond` **4** (0.25초 간격), 그대로 둠 |

**`DA_Weapon_Shotgun`이 아니다.** 이름 때문에 거기부터 고쳤다가 인게임에서 아무 변화가 없었다.
그 애셋은 아무도 안 쓴다 — 지금 캐릭터가 드는 총은 `_Test` 사본 쪽이다. 되돌려서 `Single`로 뒀다.
무기 값을 만질 때는 이름으로 고르지 말고 `BP_Unit`의 두 컴포넌트에서 거꾸로 따라갈 것:

```
BP_Unit.PaintWeapon.Profile     = DA_Weapon_Fan_T      (주무기, 샷건)
BP_Unit.SecondaryWeapon.Profile = DA_Weapon_Sniper_T   (차지샷)
```

코드는 손대지 않았다. `UPaintWeaponProfile::RepeatsWhileHeld()`가 `Automatic`과 `Continuous`를
묶어 보고, `PaintWeaponComponent`가 그걸로 유지 발사를 정한다.

`ShotsPerSecond`는 **`Single`일 때도 이미 읽히고 있었다** — 연타 제한의 하한이었다. 그래서
`Automatic`으로 바꿔도 간격이 달라지지 않고, 누르고 있을 때 저절로 반복되는 것만 바뀐다.

잉크는 한 발에 `InkCostPercent` 5%다. 계속 누르면 초당 20%, 탱크가 5초면 빈다.

---

## 유닛 — 상태 연출

### 스턴 중 머리 위 FX (`EUnitAction::Stun`)

스턴에 걸려 있는 내내 머리 위에 이펙트가 붙어 있다가 풀리면 걷힌다.

- **어디에**: 기존 **연출 피드백 시스템**(`FUnitActionFeedback`)에 붙였다. `EUnitAction`에 `Stun`을
  더하고, `AUnit`에 `StunFXComponent`와 `UpdateStunFX(bool)`를 두어
  **`AUnit::HandleStunTagChanged()`**에서 부른다.
- **왜 그 자리인가**: 스턴 태그(`State.Status.Stunned`)는 모든 머신에 복제되고, 걸릴 때와 풀릴 때
  양쪽 다 이미 이 함수로 들어온다. 스턴 소리(`Audio.Unit.Stun.Begin/End`)도 같은 자리에서 난다.
- **지속 시간을 따로 재지 않는다.** 태그가 사라질 때 끄면 그것이 곧 스턴 시간이다. 타이머를
  따로 걸면 `TryApplyStun(StunSeconds, ...)` 값과 어긋날 여지가 생긴다.
- **정리**: 대시 트레일과 같다. `Deactivate()`로 새 스폰만 멈춰 떠 있던 파티클은 수명대로
  사라지게 하고, `bAutoDestroy=true`라 꺼진 컴포넌트가 메시에 쌓이지 않는다.
  태그 이벤트가 두 번 와도 FX가 둘로 늘지 않도록 막아 뒀다.
- **대체한 것**: 없다. 스턴에 붙은 시각 연출이 아예 없었다(소리와 `BP_OnStunned`만 있었다).
- **기본값**: `Stun` 항목을 비우면 아무 일도 안 한다.
- **지금 값**: `DA_Unit_Mint` / `DA_Unit_Choco` 양쪽에
  `FX` = `NS_UnitStun`, `FXSocket` = `head`, `FXOffset` = `(0, 0, 30)`.
  소켓 기준 오프셋을 살려야 머리 위로 뜨므로 `EAttachLocation::KeepRelativeOffset`을 쓴다.

#### `NS_UnitStun` — 뜨는 시점과 사라지는 시점

`/Game/FreeParticle_SoftTofu/Niagara/NS_Sparkling_Animate_2`를
`/Game/Assets/Paint/Niagara/NS_UnitStun`으로 **복제해서** 쓴다. 원본은 그 팩의 데모 맵
(`FreeParticle_SoftTofu/Map/Overview`)도 참조하므로 건드리지 않았다.

원본 값으로는 두 가지가 문제였다. 둘 다 **유저 변수 셋만 바꿔** 고쳤다 — 그래프는 손대지 않았다.

| 유저 변수 | 원본 | 지금 | 왜 |
|---|---|---|---|
| `User.SpawnRate` | 5 /초 | **40** | 초당 5개면 별이 하나씩 늘어나 "늦게 뜨는" 것처럼 보였다. 스턴이 1초뿐이라 채워지기도 전에 끝난다 |
| `User.Lifetime Min` | 3.19초 | **0.25** | 스턴이 끝나 `Deactivate()`해도 남은 별이 3초 넘게 떠 있었다 |
| `User.Lifetime Max` | 1.78초 | **0.25** | 원본은 Min > Max로 뒤집혀 있었다 |

수명을 0.25초로 줄인 것이 곧 **0.25초 페이드아웃**이다. `Deactivate()`가 새 생성만 멈추므로,
남아 있던 별은 각자 제 수명(최대 0.25초)만큼 사라지는 커브를 타고 꺼진다. **C++은 한 줄도
안 고쳤다** — 별도의 페이드 타이머가 필요 없다.

동시에 떠 있는 별 수는 `SpawnRate × Lifetime` = 40 × 0.25 ≈ **10개**로 원본 체감과 비슷하다.
더 촘촘하게 하려면 `SpawnRate`를, 여운을 늘리려면 `Lifetime`을 올리면 된다 —
다만 `Lifetime`이 곧 사라지는 데 걸리는 시간이다.

수명이 짧아 `CurlNoiseForce`가 별을 멀리 밀 시간이 없다. 원본처럼 떠다니기보다 머리 위에서
반짝이다 꺼지는 느낌이 된다.

캐릭터 메시에 **머리 전용 소켓은 없다.** `SKM_Character_Mint`의 커스텀 소켓은 `Board`, `InkBottle`,
`Gun`뿐이라 `head` **본**에 직접 붙였다. 높이는 `FXOffset`으로 맞춘다.

### 슈퍼아머는 스턴이 끝나는 알림에 물려 있다

조사만 한 것이라 **바꾼 코드는 없다.** 스턴 값을 만지기 전에 읽을 것.

`AUnit::TryApplyStun(StunSeconds, SuperArmorSeconds)` 하나가 전부다:

- 이미 **스턴 중이거나 슈퍼아머 중이면 거부**한다. 그래서 샷건 펠릿 5발이 겹쳐 맞아도
  첫 발만 스턴을 건다
- 슈퍼아머는 타이머가 아니라 **스턴 GE가 제거되는 알림**(`OnGameplayEffectRemoved_Info`)에
  물려 시작한다. 스턴이 어떤 이유로 일찍 풀려도 이어진다
- 슈퍼아머는 밀어내기(Knockback)도 막는다
- 스턴에 걸리면 방아쇠가 풀린다 — 충전 중이었으면 **부분 발사도 없이** 그냥 날아간다
- 슈퍼아머 길이는 **충전 비율로 안 깎인다**. `StunSecondsFor`는 스턴만 곱한다

스턴 값이 사는 곳은 둘이다:

| 어디 | 무엇이 읽나 | 지금 값 |
|---|---|---|
| `FPaintDeposit.StunDuration` / `.StunSuperArmorDuration` | 무기 적중. 탄·무기 애셋마다 따로 | 샷건 0.5 / 2초, 차지샷 1.5(만충 3.0) / 3초 |
| `UItemSettings` | 광역 아이템 — 꿀풍선·히어로랜딩·꿀벌이 **공용 하나**를 본다 | 스턴 2초 / 슈퍼아머 4초 |

**광역 아이템은 그대로 둔다.** 아이템마다 다른 값을 주려면 이 둘을 `UItemProfile`로 내려야
하는데 지금 구조로는 불가능하다.

### 테스트용 스턴 큐브 (`ATestStunZone`) — 버릴 것

맵 가운데 큐브. 범위 안에 들어오면 1초 스턴, 이어서 5초 슈퍼아머.

**테스트용이라 지우기 쉽게 만들었다.** 지울 때는 이 둘만 지우면 끝이다:

```
Source/MintChoco/Sandbox/       폴더째 (TestStunZone.h / .cpp)
Lvl_Stage 의 TestStunZone_0     액터 인스턴스
```

> `bdd682a`(Sound → QA) 머지가 `Lvl_Stage`를 옛 판으로 덮으면서 이 액터가 한 번 사라졌다.
> 다시 놓으면서 이름이 `TestStunZone_Center`에서 `TestStunZone_0`으로 바뀌었다(엔진이 붙인 이름).
> 값과 위치는 같다. 위의 [머지 후 확인 목록](#머지-후-확인-목록)을 볼 것.

그러려고 아래를 전부 피했다 — 기존 파일 수정 **0줄**, 새 게임플레이 태그 없음, 새 열거형 값 없음,
설정 항목 없음, `/Game`에 새 에셋 없음, 공용 데이터 에셋 참조 없음. 외형은 엔진 기본
`/Engine/BasicShapes/Cube`를 그대로 쓴다.

- **어디에**: 아무 데도 붙지 않는다. `AUnit::TryApplyStun(StunSeconds, SuperArmorSeconds)`를
  바깥에서 부르기만 한다 — 이미 공개된 함수다.
- **동작**: `OnComponentBeginOverlap` 하나. 들어오는 순간 한 번만 걸고, 범위 안에 머무르는
  동안은 다시 걸지 않는다(나갔다 들어와야 한다). 슈퍼아머 5초 동안은 `TryApplyStun`이 스스로
  거절하므로 사실상 재발동 쿨다운 노릇도 한다.
- **아이템 스턴과 같은 규칙**을 탄다. 스턴 규칙이 나중에 바뀌어도 따라간다.
- 복제하지 않는다. 레벨 배치 액터라 모든 머신에 있고, 스턴은 서버가 걸어 GAS가 복제한다.
- **지금 값**: `TriggerRadius` 400cm, `StunSeconds` 1, `SuperArmorSeconds` 5.
  위치 `(0, 0, 409.5)` — `Lvl_Stage`의 페인트 가능 액터 31개 경계가 X/Y −4000~4000이라
  중심이 정확히 원점이고, 그 자리 바닥이 z 359.5라 100cm 큐브가 바닥에 닿게 +50 했다.
  참고로 양 팀 스폰 지점 중점도 `(0, 0, 460.5)`로 같은 XY다.

큐브를 키우려면 액터가 아니라 **Mesh 컴포넌트의 스케일**을 올릴 것. 액터를 키우면 루트인
구체 트리거의 반경까지 같이 커진다.

---

## 유닛 — 시야와 이동

### 시야 피치 제한 복구 (`AUnit::ApplyViewPitchLimits`)

머지가 떨어뜨렸던 코드를 그대로 되살렸다. 원래는 `f360cba`(09-10)가 넣었고, `b69ecd3f`
("game-effect 병합", 09-13)에서 사라져 `Source/` 어디에도 `ViewPitch`라는 글자가 없었다.

| | |
|---|---|
| 어디에 | `AUnit::ViewPitchMin` −45도 / `ViewPitchMax` +60도, `SetupPlayerInputComponent` 시작에서 호출 |
| 무엇을 | 소유 클라이언트의 `APlayerCameraManager`에 두 값을 옮긴다 |
| 대체한 것 | 없음(복구). 그전에는 위아래로 89도까지 꺾였다 |

`SetupPlayerInputComponent`에서 부르는 이유는 그곳이 로컬 조종 폰이 확정되는 유일한 지점이고,
리스폰하면 다시 불려 새 카메라 매니저에도 자동으로 걸리기 때문이다. 서버는 이미 클램프된
회전을 `ServerMove`로 받으므로 따로 걸지 않는다.

`InputConfig`가 비어 이른 반환을 타더라도 시야 제한은 걸리도록 **호출을 그 반환보다 앞에** 뒀다.

### 점프 밸런스 — 중력으로 체공을 깎고 높이로 역할을 나눈다

높이 = v² / 2g, 체공 = 2v / g. 중력만 올리면 둘 다 줄고, 속도를 같이 올리면 높이는 살리면서
체공만 준다.

| | v | 최고 높이 | 체공 | 수평 도달 | 역할 |
|---|---|---|---|---|---|
| 그냥 점프 | 660 | 111cm | 0.67초 | 6.7m | 턱 넘기 |
| 대쉬 점프 | 660 | 111cm | 0.67초 | 11.4m | 같은 높이로 더 멀리 |
| 점프대 | 1750 | 781cm | 1.79초 | — | 높은 곳 오르기 |

대쉬 점프의 높이는 **일부러 그냥 점프와 같게 뒀다**(둘 다 660). 차이는 수평 거리뿐이다.
`DashJumpZVelocity`는 나중에 따로 낮출 수 있도록 열어 둔 자리다.

바뀐 값 (`GravityScale` 2.0 = 1960 cm/s² 기준):

| 어디 | 값 | 전 |
|---|---|---|
| `BP_Unit` → `CharMoveComp.GravityScale` | 2.0 | 1.0 |
| `BP_Unit` → `CharMoveComp.JumpZVelocity` | 660 | 420 |
| `BP_Unit` → `CharMoveComp.AirControl` | 0.15 | 0.05 |
| `UUnitMovementComponent::DashJumpZVelocity` | 660 | 없음(새 값) |
| `BP_JumpPad.velocity.Z` — CDO와 **레벨 인스턴스 8개 전부** | 1750 | 1100 |

전에는 그냥 점프와 대쉬 점프의 높이·체공이 **완전히 같았다**(둘 다 `JumpZVelocity` 하나를 썼다).
차이가 수평 속도뿐이라 역할이 안 나뉘었다.

### 대쉬 점프 분리 (`DashJumpZVelocity`)

| | |
|---|---|
| 어디에 | `UUnitMovementComponent::DoJump(bool, float)` 오버라이드 |
| 무엇을 | 대시 중이면 `JumpZVelocity` 대신 `DashJumpZVelocity`로 뛴다. 0 이하면 분리하지 않는다 |
| 대체한 것 | 대시 여부와 무관하게 `JumpZVelocity` 하나를 쓰던 엔진 기본 동작 |

`Velocity.Z`를 직접 쓰지 않고 **값만 잠깐 바꿔 `Super::DoJump`를 태운다.** 직접 쓰면
`CanAttemptJump`·평면 구속·플랫폼 기준 속도를 하나씩 다시 구현하게 된다.

압축 플래그를 새로 만들지 않았다: 대시 의도(`bWantsToDash`)가 이미 무브에 실려 오고 보정 후
리플레이에서도 되살아나므로, 그 값을 보고 고르면 서버와 클라이언트가 같은 답을 낸다.

### 보드 속도 (`DashSpeedMultiplier`)

| | |
|---|---|
| 어디에 | `BP_Unit` → `CharMoveComp` |
| 무엇을 | `DashSpeedMultiplier` 1.7 → **2.0** |
| 대체한 것 | 1.7 |

`MaxWalkSpeed` 1000이 미도색 걷기다. 그래서 미도색 보드가 정확히 2000이 된다.

### 발밑 색이 속도와 잉크를 정한다

| | |
|---|---|
| 어디에 | `FPaintCellGrid`, `UPaintableComponent`, `UPaintSubsystem`, `UUnitMovementComponent`, `UInkTankComponent` |
| 무엇을 | 딛고 있는 바닥의 색으로 이동 속도와 잉크 회복에 배율을 건다 |
| 대체한 것 | 바닥과 무관하게 `MaxWalkSpeed` 하나, `RefillPerSecond` 하나 |

**먼저 없는 것을 만들었다.** "이 지점의 바닥이 누구 색인가"를 묻는 API가 프로젝트에 없었다.
`FPaintCellGrid`에는 쓰기(`Mark`)와 전체 집계(`GetFraction`)만 있었다.

```
FPaintCellGrid::PaintIdAt(로컬 위치, 로컬 방향)     격자 밖이거나 면이 없으면 PaintIdNone
UPaintableComponent::GetPaintIdAt(월드 위치, 월드 법선)
UPaintSubsystem::GetPaintIdAtHit(히트)              광선을 안 쏜다. 이미 있는 히트를 읽는다
UPaintSubsystem::GetPaintIdUnder(위치, 깊이)        히트가 없는 곳에서 쓰라고 한 번 내린다
```

`VoxelOf`를 재사용하지 않았다. 그쪽은 격자 안으로 **잘라 넣어서** 바깥 점이 가장자리 색을
물려받는다. 조회는 바깥이면 바깥이라고 답해야 한다.

표면 위의 점이 복셀 경계에 걸리면 면이 없는 칸이 나오므로, `GetPaintIdAt`이 실패하면
법선 반대로 셀의 1/4만큼 밀어 한 번 더 본다.

**광선을 쏘지 않는다.** 무브먼트는 엔진이 이미 들고 있는 `CurrentFloor.HitResult`를 넘긴다.
`UpdateCharacterStateBeforeMovement`에서 무브마다 한 번 구해 `FloorPaintId`에 담으므로
`GetMaxSpeed`가 몇 번 불려도 값은 한 번만 계산된다. 저장 무브에 실을 것도 없다 — 리플레이가
같은 바닥을 재현하면 이 값도 같이 재현된다.

프록시도 갱신한다. 프록시는 위치를 복제로 받지만 **속도는 스스로 시뮬레이션**하므로,
빼먹으면 남의 캐릭터만 상대 색 위에서 제 속도로 달린다.

**배율은 한 곳에서 나온다.** 속도와 잉크가 같은 것을 쓴다. 두 곳에서 따로 계산하면 표가 어긋난다.

| `UUnitMovementComponent` | 값 | |
|---|---|---|
| `OwnFloorMultiplier` | **1.5** | 내 색 |
| (미도색) | 1.0 | 고정 |
| `EnemyFloorMultiplier` | **0.5** | 상대 색 |
| `EnemyFloorDashMultiplier` | **1.0** | 상대 색 위 보드 — 가속이 무의미하다 |
| `DashSpeedMultiplier` | 2.0 | 그 밖의 바닥에서 보드 |

**스피드 스타는 바닥을 무시한다.** 부스트 플래그가 서 있으면 바닥 배율이 내 색(1.5)으로 굳고
보드 배율도 2.0으로 굳는다. 상대 진영 한복판에서도 같은 속도가 나오는 것이 이 아이템이
보장하는 최소 속도다.

이동 속도 (`MaxWalkSpeed` 1000 = 미도색 걷기):

| | 걷기 | 보드 |
|---|---|---|
| 내 색 | 1500 | 3000 |
| 미도색 | 1000 | 2000 |
| 상대 색 | 500 | 500 |
| 스타 | 2250 | 4500 |

잉크 회복 (`BP_Unit.InkTank.RefillPerSecond` **0.10**. C++ 기본값은 0.15지만 `BP_Unit`이
이미 0.10으로 덮어 두고 있었다 — 바꿀 것이 없었다):

| | 걷기 | 보드 |
|---|---|---|
| 내 색 | 15%/s | 30%/s |
| 미도색 | 10%/s | 20%/s |
| 상대 색 | 5%/s | 5%/s |

미도색 보드 20%와 상대색 보드 5%는 기획표에 없어 같은 규칙으로 채운 값이다.

`GetInkRefillMultiplier`는 두 배율의 곱이되 **부스트 배율은 뺀다.** 별을 먹었다고 잉크가
1.5배로 차지는 않는다. 다만 바닥은 내 색으로 굳으므로 상대 진영에서 별을 먹으면 잉크도 15%/s가 된다.

잉크는 **서버만 채운다**(`Refill`이 `ROLE_Authority`에서만 불린다). 그래서 예측할 것이 없고,
배율이 클라이언트와 갈라져도 고무줄이 나지 않는다.

**예측이 안전한 이유:** `ApplySplat`이 스플랫 로그를 타고 모든 머신에서 돌아 `CellGrid.Mark`가
서버와 클라이언트에서 같이 일어난다. 조회가 읽는 칸이 `Mark`가 쓰는 칸과 **같은 칸**이므로
양쪽이 같은 답을 낸다. 이것이 이동 속도를 여기에 걸 수 있는 근거다.

테스트: `MintChoco.Items.Movement.SpeedBoost`(값이 새 규칙으로 바뀜),
`MintChoco.Items.Movement.InkRefillMultiplier`(새로 추가).

---

## 경기 진행

### 경기 시간 (`MatchDuration`)

| | |
|---|---|
| 어디에 | `BP_GameMode` CDO |
| 무엇을 | `MatchDuration` 90 → **180초** |
| 대체한 것 | 90초 |

`CountdownDuration` 0, `ItemSpawnInterval` 3은 그대로다.

---

## 아이템 — 초콜릿 분수

### 넘치는 발밑 도포 (`BurstCount` / `BurstInterval` / `BurstGrowth`)

설치 순간 한 번만 뿌리던 것을 1초마다 점점 넓게 네 번 뿌리도록 바꿨다.

```
t=0s   1.0배      설치와 동시에
t=1s   1.4배
t=2s   1.96배
t=3s   2.74배     여기서 끝, 돔도 같이 사라진다
```

| | |
|---|---|
| 어디에 | `UChocolateFountainProfile`에 값 셋, `AChocolateFountain::StartGroundBursts` / `FireGroundBurst` |
| 무엇을 | 돔이 타이머를 들고 `APaintBurst`를 `BurstCount`번 뿌린다. 한 번마다 반경 배율에 `BurstGrowth`를 곱한다 |
| 대체한 것 | `UGA_ChocolateFountain::OnItemActivated`의 `APaintBurst::Spawn` 한 줄 |
| 값 | `BurstCount` 4, `BurstInterval` 1.0초, `BurstGrowth` 1.4, `DA_Item_ChocolateFountain.Lifetime` 5 → **3초** |

**일정을 능력이 아니라 돔이 들고 있다.** 초콜릿 분수는 즉발(`Duration` 0)이라 능력 인스턴스가
곧 끝나 타이머를 얹을 자리가 없다. 돔은 정확히 `Lifetime` 동안 살고 `EndPlay`에서 타이머를
걷으므로 마지막 도포와 돔의 끝이 저절로 맞는다.

**돔 크기는 안 커진다.** 커지는 것은 바닥 도포뿐이다. 돔은 탄을 막는 벽이라 크기가 변하면
전투 판정이 흔들린다.

#### 배율이 속도에 닿는 두 가지 경로

`DA_Item_ChocolateFountain`은 `bBurstMatchesRadius`가 **거짓**이다(`Radius` 900, `Burst.Speed` 900,
`MinPitch` = `MaxPitch` = 0). 그래서 `MakeBurst`가 두 갈래다.

| `bBurstMatchesRadius` | 배율이 곱해지는 곳 | 왜 |
|---|---|---|
| 참 | `Radius`. `SpeedForRange`가 v² 관계로 환산 | 45도 포물선의 사거리는 v²에 비례 |
| 거짓 (지금) | `Speed`에 그대로 선형 | 수평으로 쏘면 떨어지는 시간이 속도와 무관 → 거리가 v에 비례 |

#### 마지막 한 번을 수명 안으로 당긴다

`(BurstCount − 1) × BurstInterval`이 `Lifetime`과 같으므로 마지막 도포와 `SetLifeSpan`이
같은 프레임에 온다. **두 타이머의 순서는 정해져 있지 않다.** 그래서 남은 시간이 간격보다
짧으면 0.05초 여유를 두고 앞당겨 예약한다. 가장 넓게 칠하는 마지막 한 번이 안 나가면 안 된다.

---

## 아이템 — 스폰

### 경기 시작에 9곳 전부, 먹은 자리만 개별 리스폰

| | |
|---|---|
| 어디에 | `Lvl_Stage`의 `BP_ItemSpawnPoint` 인스턴스 9개 |
| 무엇을 | `SpawnMode` `Shared` → **`Standalone`**, `RespawnDelay` 3 → **10초** |
| 대체한 것 | 게임모드가 15초마다 빈 지점 하나를 골라 놓던 주기 스폰 |

**게임모드 코드는 한 줄도 안 고쳤다.** `AGameGameMode::StartItemSpawning`은 `Shared` 지점만
목록에 담으므로, 전부 `Standalone`이 되면 목록이 비고 주기 타이머가 스스로 안 돈다.
`Standalone` 지점은 `BeginPlay`에서 자기 픽업을 놓고, 픽업이 사라지면 `RespawnDelay` 뒤
다시 놓는다.

지점은 **11개다.** 처음 조사했을 때는 9개였고 그 뒤 두 개가 늘었다.

### 빛 기둥 (`AItemPickup::Pillar`)

| | |
|---|---|
| 어디에 | `AItemPickup`에 `UNiagaraComponent Pillar` + `PillarTemplate` + `PillarSeconds` |
| 무엇을 | 픽업이 **활성이 되는 순간** 켜고 누가 가져갈 때까지 계속 선다 |
| 대체한 것 | 없음(새 기능) |
| 값 | `PillarSeconds` **0초**, `BP_ItemPickup.PillarTemplate` = `NS_ItemPillar` |

`PillarSeconds`가 5초였을 때는 상자가 그대로 있는데 표시만 먼저 꺼져, 멀리서 보면 아이템이
이미 없어진 것처럼 보였다. **0으로 두면 타이머를 아예 걸지 않는다**(`StartPillar`가
`if (PillarSeconds > 0.0f)`로 거른다). 끄는 것은 `OnRep_Collected`의 `StopPillar()`뿐이라
먹는 순간에 맞춰 사라진다. 코드는 그대로고 값만 바꿨다.

기존 `Laser`와 **다른 것이다**: `Laser`는 나타나기 *전* 예고(`Announced` 상태),
기둥은 나타난 *뒤* 표시(`Active` 상태)다.

기둥을 지점이 아니라 **픽업이 들고 있다.** 빛 기둥은 "여기 아이템이 새로 생겼다"는 신호라
픽업의 수명과 정확히 묶여 있어야 하고, 픽업이 사라지면 같이 사라져야 한다.

`bAutoActivate`는 꺼 두고 `StartPillar`가 직접 켠다. 자동 활성이면 템플릿이 비어 있어도
켜지려 들고, 예고 상태와 데디케이티드 서버에서까지 돈다(돔의 거품과 같은 규칙).
`ApplyState`는 여러 번 불리므로(`BeginPlay`, `OnRep_State`) `bPillarStarted`로 한 번만 켠다.

#### `NS_ItemPillar` — `NS_JumpPad`의 복제본

새로 그리지 않고 점프대 이펙트를 복제했다. 위로 솟는 띠(`Bands`) + 글로우(`Glow_Base`) +
포자(`Spores`) 구조가 빛 기둥에 그대로 맞고, `User.Color` 하나로 색이 정해진다.

`User.Color`를 점프대의 주황(0.71, 0.21, 0.04)에서 **밝은 금색(1.0, 0.92, 0.45)** 으로 바꿨다.
그대로 두면 점프대와 구분이 안 된다.

**색은 아이템마다 나누지 않는다.** 놓이는 것은 무작위 상자이고 무엇이 들었는지는 먹은 뒤에야
정해져 보인다. 기둥 색이 내용물을 알려주면 그 재미가 없어진다.

세 이미터의 수명이 제각각이라 **켜 둔 채로 두면 띠가 먼저 죽고 바닥 글로우만 남는다.**
`Deactivate()` 뒤 잔상이 오래 가는 것도 이 때문이다.

| 이미터 | 메시 | 파티클 수명 | 스폰 |
|---|---|---|---|
| `Bands` | `SM_CircularBand` (폭 140cm, 높이 13cm) | 0.75초 | 5/초 |
| `Spores` | 스프라이트 (GPU) | 0.5~0.8초 | 100/초 |
| `Glow_Base` | `SM_CircularGlow` (폭 173cm, 높이 18cm) | 5초 | 루프(1초)마다 1개 |

메시는 **둘 다 납작한 원반이다.** 기둥처럼 보이는 것은 `Bands`가 0.75초 동안 솟으면서
쌓이는 착시이지 세로로 긴 메시가 아니다.

### 상자 반짝임 (`AItemPickup::BoxSparkle`)

| | |
|---|---|
| 어디에 | `AItemPickup`에 `UNiagaraComponent BoxSparkle` + `BoxSparkleTemplate` + `BoxSparkleHeight` |
| 무엇을 | 픽업이 활성이 되는 순간 켜고 누가 가져갈 때 끈다. 시간 제한이 없다 |
| 대체한 것 | 없음(새 기능) |
| 값 | `BoxSparkleHeight` 60cm, `BP_ItemPickup.BoxSparkleTemplate` = `NS_ItemBoxSparkle` |

기둥과 완전히 같은 규칙으로 돈다: `bAutoActivate` 꺼 두고 `StartBoxSparkle`이 직접 켜며,
`bBoxSparkleStarted`로 한 번만 켜고, 데디케이티드 서버에서는 켜지 않는다. 다른 점은 **끌 시각을
예약하지 않는다**는 것뿐이다.

높이 60cm는 상자 메시(`BP_ItemPickup:Mesh`, `SM_Egg`)의 Z 오프셋과 같다. 상자는 위아래로
흔들리지만(`BobAmplitude` 10cm) 반짝임은 제자리에 둔다 — 메시에 붙이면 반짝임까지 같이 출렁인다.

#### `NS_ItemBoxSparkle` — `NS_Sparkling`의 복제본

원본(`FreeParticle_SoftTofu` 팩)은 데모 맵이 참조하므로 손대지 않고 복제해서 고쳤다.

| | 원본 | 지금 | 왜 |
|---|---|---|---|
| `bFixedBounds` | false | **true**, ±120cm | 아래 참고 |
| `User.Sphere Radius` | 133cm | 90cm | 트리거 반경 80cm를 살짝 감싸게 |
| `User.Lifetime Min` | 3.19초 | 0.8초 | 원본이 Min > Max로 뒤집혀 있었다 |
| `User.Lifetime Max` | 1.78초 | 1.4초 | 같은 이유 |
| `User.Uniform Sprite Size Min` | 35.6 | 110 | 원본 크기로는 상자 옆에서 티가 안 난다 |
| `User.Uniform Sprite Size Max` | 56.4 | 180 | 같은 이유 |
| `User.SpawnRate` | 44.1 | 35 | 커진 만큼 수를 줄여 지저분해지지 않게 |

**GPU 전용 나이아가라는 `bFixedBounds` 없이는 통째로 안 그려진다.** 이 시스템은 이미터가
`HangingParticulates` 하나뿐이고 그것이 GPU 시뮬이다. GPU 이미터는 바운드를 스스로 돌려주지
못해서, 고정 바운드가 없으면 프러스텀 컬링에 걸려 화면에서 사라진다. `NS_ItemPillar`가 멀쩡한
이유가 이걸 뒷받침한다 — 거기엔 CPU 이미터(`Bands`, `Glow_Base`)가 있어 걔들이 바운드를
만들어 주고, 덕분에 GPU인 `Spores`도 같이 그려진다.

이미터의 `Loop Behavior`는 `Infinite`다. 끄기 전까지 계속 돈다.

`User.Color`는 원본 그대로 (555, 255, 55)다. 1을 한참 넘는 HDR 값이라 블룸이 셀 수 있다.

**크기와 밝기가 같이 맥동한다.** `ScaleSpriteSize.Scale Factor`와 `ScaleColor.Scale RGB`가
둘 다 `Abs(Sine(Emitter.Age, Period))`를 쓰고, `Period`는 스폰할 때 입자마다
`User.Sparkling_Speed_Min`~`Max`(0.16~0.64초) 중에서 뽑힌다. 그래서 각 입자가 제 속도로
반짝인다. 순간적으로 0에 가까워지는 입자가 늘 섞여 있으므로, 한 장의 스크린샷으로 밝기를
판단하면 안 된다.

### ★ 새 native UPROPERTY의 기본값은 블루프린트를 **컴파일**해야 인스턴스에 간다

`BoxSparkleTemplate`을 MCP(`ObjectTools.set_properties`)로 CDO에 써 넣고 저장했더니
`Default__BP_ItemPickup_C`를 다시 읽으면 값이 보이고 애셋 레지스트리에도 의존이 잡히는데,
**런타임에 스폰된 인스턴스에서는 계속 `None`이었다.** 그래서 `StartBoxSparkle`이 템플릿 가드에
걸려 아무것도 안 켰다. 같은 자리의 `PillarTemplate`(사람이 BP 에디터에서 넣은 값)은 멀쩡히 왔다.

`BlueprintTools.compile_blueprint`를 한 번 부르자 바로 인스턴스까지 값이 갔다.

**C++에 새 UPROPERTY를 추가하고 MCP로 그 기본값을 넣었으면, 저장만 하지 말고 블루프린트를
컴파일할 것.** 확인은 CDO가 아니라 PIE로 스폰된 인스턴스에서 한다.

---

## 맵 — 바닥 칠

### 메인 바닥의 Allow CPU Access (`Boolean_483BB222`)

| | |
|---|---|
| 어디에 | `Content/Maps/_GENERATED/User/Boolean_483BB222` — `Lvl_Stage`의 `BP_PaintableCube31`이 쓰는 바닥 메시 |
| 무엇을 | `bAllowCPUAccess` false → **true** |
| 대체한 것 | 에디터에서는 칠해지는데 **패키지 빌드에서만 바닥이 안 칠해지던** 증상 |

`PaintAtlasBaker`는 LOD 0의 **CPU 사본**을 읽어 아틀라스를 굽는다. 에디터는 CPU 지오메트리를
항상 들고 있어 플래그와 무관하게 읽히지만, **쿡된 빌드는 `bAllowCPUAccess`가 켜진 에셋만 CPU
사본을 남긴다.** 그래서 에디터만 멀쩡하고 패키지만 깨지는 한쪽짜리 증상이 된다
(`PaintMeshTriangles.cpp:31`이 찍는 `mesh geometry is not CPU-readable ... paint disabled.`).

이 메시는 모델링 툴이 Boolean으로 만들어 `_GENERATED/User/`에 저장한 것이고, **그렇게 생성된
스태틱 메시는 이 플래그가 기본으로 꺼져 있다.** 같은 폴더의 `Boolean_AAC84092`는 이미 켜져
있었고, `Merge_7C718DE5`는 참조가 0건인 고아라 건드리지 않았다.

이 바닥 하나가 `37,882,136 cm^2`(3,788 m2, 셀 그리드 320 x 320)로 스테이지 페인트 면적
8,660 m2의 약 44%다. 나머지 30개 페인터블은 전부터 정상이었다.

패키지 로그로 확인한 전후:

| | 전 (`Unreal3D4`) | 후 (`Unreal3D5`) |
|---|---|---|
| `not CPU-readable` 경고 | 2 | **0** |
| `cell grid` 줄 | 30 | **31** |
| `baking a paint atlas` | 18 | **19** |

**새 페인터블을 모델링 툴로 만들면 이 플래그부터 켜라.** 에디터에서만 테스트하면 절대
드러나지 않고, 패키징해야만 나온다.

---

## 맵 — 풍선

### 파열 탄 교체 (`DA_Paintball_BalloonBurst`)

| | |
|---|---|
| 어디에 | `Lvl_Stage`의 `BP_Balloon` **9개 전부** (인스턴스 값이다) |
| 무엇을 | `BurstPaintball` `DA_Paintball_Heavy` → **`DA_Paintball_BalloonBurst`**, `BurstCount` 24 → **8** |
| 대체한 것 | 자국이 너무 작아 24발을 뿌려도 티가 안 났다 |

`DA_Paintball_BalloonBurst`는 **`DA_Paintball_Bomb`(디저트 폭격 탄)의 복사본**이다. 원본을
공유하면 풍선을 만질 때마다 폭격이 같이 움직인다.

| | 브러시 | volume | 자국 반경 |
|---|---|---|---|
| 전 (`DA_Paintball_Heavy`) | `DA_Brush_Mop` (Base 40) | 1.6 | **50.6cm** |
| 후 (`DA_Paintball_BalloonBurst`) | `DA_Brush_Bomb` (Base 150) | 3.0 | **260cm** |

반경이 5배, 넓이가 26배다. 그래서 발수를 24 → 8로 줄여도 칠해지는 양은 훨씬 늘어난다.

**CDO만 고치면 안 된다.** 레벨에 놓인 9개가 각자 값을 들고 있다. `BurstSpeed` 200,
`MaxHealth` 20은 그대로다.

---

## 아이템 — 연출

### 꿀벌에 따라붙는 해골 (`BP_Bee.BodyFX`)

| | |
|---|---|
| 어디에 | `BP_Bee`의 `BodyFX`(= `AItemProjectile::BodyFX`) |
| 무엇을 | 꿀벌이 나는 동안 해골 반짝임이 몸을 감싸고 같이 날아간다 |
| 대체한 것 | 없음. 비어 있던 슬롯이다 |
| 값 | `BodyFX` = `NS_BeeSkull`, `BodyFXScale` 1 |

**컴포넌트를 새로 달지 않는다.** `AItemProjectile`에 이미 이펙트 슬롯이 둘 있다:
`TrailFX`(뒤로 끌리는 꼬리)와 `BodyFX`(공을 감싸고 같이 나는 것). 해골은 뒤쪽이다.
둘 다 `Mesh`의 같은 자리에 붙으므로 함께 켜도 어긋나지 않는다.

**`TrailFX`는 main 것이다** — `NS_ArrowTrail_Magic`, 스케일 1. 꿀벌 트레일은 main이 따로
붙였고 이쪽은 그 위에 얹힌다. 건드리지 말 것.

처음에는 `SkullFX`라는 `UNiagaraComponent`를 루트에 직접 달았는데, main 머지에서 `BP_Bee`가
충돌해 main 쪽이 통째로 채택되며 사라졌다. 다시 달면서 컴포넌트 대신 이 슬롯을 쓴 이유:
**`BodyFX`는 클래스 디폴트라 모든 머신이 갖고 있고**(프로필은 서버만 받는다), 복제 액터의
메시 보간을 그대로 타므로 클라이언트에서도 공과 같이 움직인다. 컴포넌트를 새로 달면
`BP_Bee`가 또 충돌했을 때 같은 일이 반복된다.

#### `NS_BeeSkull` — `NS_Sparkling_Skull`의 복제본

원본도 `NS_Sparkling`과 같은 팩 결함을 그대로 갖고 있었다: GPU 전용에 고정 바운드 없음,
`Lifetime Min`(2) > `Max`(1) 역전.

| | 원본 | 지금 | 왜 |
|---|---|---|---|
| `bLocalSpace` | false | **true** | 벌에 붙어 다니게. 월드 스페이스면 지나간 자리에 꼬리로 남는다 |
| `bFixedBounds` | false | true, ±70cm | GPU 전용이라 없으면 통째로 안 보인다 |
| `User.Sphere Radius` | 100 | 45 | 벌 몸(루트 반경 30)을 감싸게 |
| `User. Size Min` / `Size Max` | 30.3 / 68.2 | 18 / 32 | 벌보다 커 보이지 않게 |
| `User.Lifetime Min` / `Max` | 2 / 1 (역전) | 0.35 / 0.7 | 짧아야 벌 곁에 머문다 |
| `User.SpawnRate` | 33 | 25 | |
| `User.Noise Strength` | 155 | 60 | 900cm/s로 나는 벌에서 흩어지지 않게 |

이미터 이름은 `HangingParticulates` 하나뿐이다(GPU 스프라이트).

---

## 오디오

### 새 사운드 7개 연결

| 상황 | 사운드 | 어디에 | 대체한 것 |
|---|---|---|---|
| 보드 올라탈 때 | `보드_생성` | `DA_SoundBank` → `Audio.Unit.Dash` | `효과음_보드_생성` |
| 보드 주행 중 | `보드_주행` (루프) | `DA_SoundBank` → `Audio.Unit.Board.Loop` | 없음 (신규) |
| 샷건 발사 | `샷건_발사음` | `DA_Sound_Shotgun` → `Audio.Weapon.Fire` | 없음 (기본 뱅크는 그대로) |
| 아이템 획득 | `아이템_획득_효과음` | `DA_SoundBank` → `Audio.Item.Pickup` | `효과음_아이템_획득` |
| 페인트볼 착탄 | `잉크칠해지는음` (볼륨 0.5) | `DA_SoundBank` → `Audio.Weapon.Impact` | 없음 (비어 있었음) |
| 차지샷 발사 | `차지샷_발사음` | `DA_Sound_ChargeShot` → `Audio.Weapon.Fire` | 없음 |
| 차지샷 충전 | `차지샷충전음` | `DA_SoundBank` → `Audio.Weapon.ChargeLoop` | `효과음_차지_공격_준비시` |

발사음을 기본 뱅크가 아니라 **무기별 오버라이드 뱅크**에 둔 이유: 기본 `Audio.Weapon.Fire`를
샷건 소리로 바꾸면 아이템 무기(스피너 등)도 같이 바뀐다. 오버라이드 뱅크는
`DA_Sound_SweetSpinner`를 복제해 만들었고, 무기 프로필의 `Sounds`에 꽂았다 —
**원본과 `_T` 양쪽 모두**.

`Audio.Weapon.Impact`를 고르고 `Audio.World.Splat`을 피한 이유: `World.Splat`은
`PaintSubsystem`에서 **스플랫 하나마다** 울린다(궤적까지 켜면 한 발에 수십 번).
`Weapon.Impact`는 `APaintProjectile`에서 공이 닿을 때만 울린다.

`보드_주행`은 `bLooping`을 켰다(길이 2.46초). `차지샷충전음`은 길이 3.10초로 `ChargeTime` 3초와
거의 같아 루프를 켜지 않았다 — 충전이 끝나면 `StopChargeFX`가 끊는다. 무한탄환 아이템이 붙으면
충전이 0.5초로 짧아져 소리가 중간에 잘린다.

### 보드 주행 루프 (`Audio.Unit.Board.Loop`)

- **어디에**: `AudioTags`에 태그 추가, `SoundBankTest`의 필수 태그 목록에 추가,
  `AUnit`에 `BoardAudioComponent`와 `UpdateBoardLoopSound()`를 더해 **`SetBoardShown()`**에서 부른다.
- **왜 `SetBoardShown`인가**: 보드는 대시 키가 아니라 애님 상태 기계를 따른다
  (`UpdateDashEffects`의 주석이 그렇게 못박아 뒀다). 보드가 실제로 보이는 동안만 소리가 나야 한다.
  카메라 페이드는 보지 않는다 — 페이드는 그림만 감추고 보드는 여전히 달리는 중이다.
- **방식**: 차지 루프와 같다. `PlayAttached`가 돌려주는 `UAudioComponent*`를 들고 있다가 `Stop()`.
- **대체한 것**: 없다. 루프용 태그도 코드도 없었다.

### 산탄 착탄음 한 발에 한 번 (`bImpactSoundOncePerShot`)

- **어디에**: `APaintProjectile`에 `bPlaysImpactSound` + `SetPlaysImpactSound()`,
  `UPaintGunProfile::Launch`가 첫 펠릿을 뺀 나머지의 소리를 끈다.
- **왜**: 펠릿 5발이 거의 동시에 닿아 같은 소리가 겹쳤다. `USoundConcurrency` 에셋이 프로젝트에
  하나도 없고 MCP로는 새 에셋을 만들 수 없어(복제만 가능) 코드로 풀었다.
- **풀에서 재사용될 때**: `Init`이 매번 `true`로 되돌린다. 끄는 쪽은 `Launch`가 돌아온 뒤에 건다.
- **대체한 것**: 없다. 칠하는 양과 이펙트는 5발 그대로다.
- **기본값**: 꺼짐. **지금 값**: `DA_Weapon_Fan` 양쪽에 켜짐.

### 사운드뱅크 테스트 현황

`MintChoco.Audio.Bank`는 **Fail**이다. 빈 슬롯 20개 때문인데 **전부 이번 작업 이전부터 비어 있던
것들**이다(작업 전 24개 → 4개를 채워 20개). 연결한 7개는 전부 통과했다.

아직 비어 있으면서 **쓸 만한 에셋이 이미 있는** 것들:

| 빈 태그 | 후보 에셋 |
|---|---|
| `Audio.Unit.Land` | `효과음_플레이어_보드_착지`, `효과음_칠해진_바닥_착지_플레이어_보드_` |
| `Audio.Match.Start` | `효과음_게임321시작` |
| `Audio.Match.CountdownTick` | `효과음_KO카운트다운` |
| `Audio.Match.End.Draw` | `효과음_경기_종료` |
| `Audio.Item.Announce` | `효과음_아이템_룰렛` |
| `Audio.Weapon.ChargeReady` | `효과음_차지발사1` / `효과음_차지발사2` |

---

## 머지 후 확인 목록

**이 저장소에서 머지가 코드와 값을 떨어뜨린 사고가 다섯 번 있었다.** 머지 직후 아래를 확인한다.

### 왜 생기나

| 종류 | 왜 | 드러나는가 |
|---|---|---|
| `.uasset` / `.umap` | **바이너리라 git이 줄 단위로 합치지 못하고 한쪽을 통째로 고른다.** 옛 조상에서 갈라진 브랜치가 같은 파일을 건드리면 그쪽이 이긴다 | **안 드러난다.** PIE에서 값이 되돌아간 것으로만 안다 |
| C++ | 머지가 한쪽 hunk를 버린다. 선언만 사라지고 정의가 남기도 한다 | 운이 좋으면 컴파일 에러 |

`git log -S`로는 안 잡힌다 — 머지가 떨어뜨린 코드는 `-S`의 기본 히스토리 단순화에서 빠진다.
`git log -- <경로>`도 마찬가지다: 머지가 채택하지 않은 쪽의 커밋은 그 파일 이력에 안 나온다.

### 검사

```bash
# 1. 되돌아간 파일 찾기 (머지 전 내 커밋 ↔ 머지 결과)
git diff --name-status <머지전_내커밋> HEAD -- Content/Maps Content/LevelPrototyping Content/Blueprints Content/Game/Data

# 2. C++ 은 빌드가 잡아 준다. 반드시 머지 직후 리빌드할 것
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" MintChocoEditor Win64 Development -Project="D:\GitHub\MintChoco\MintChoco.uproject" -WaitMutex -FromMsBuild
```

**리빌드 없이 에디터를 띄우면 안 된다.** 새 C++ 클래스가 DLL에 없어 그것을 부모로 삼는 애셋이
로드에 실패하고(`Failed to load Class /Script/MintChoco.X as Parent for ...`),
**그 상태로 저장하면 끊긴 연결이 디스크에 굳는다.**

### 지금 맞는 값 (되돌아갔는지 볼 기준)

| 어디 | 있어야 할 값 |
|---|---|
| `BP_JumpPad.velocity.Z` — CDO와 **레벨 인스턴스 8개 전부** | **1750** (되돌아가면 1100) |
| `Lvl_Stage`의 `BP_ItemSpawnPoint` **11개** | `SpawnMode` **Standalone**, `RespawnDelay` **10초** (되돌아가면 Shared / 3초) |
| `Lvl_Stage`의 `TestStunZone` | **버렸다.** 59215e2 머지에서 사라진 것을 그대로 두기로 했다 |
| `BP_Unit` → `CharMoveComp` | `GravityScale` 2.0, `JumpZVelocity` 660, `DashJumpZVelocity` 660, `AirControl` 0.15, `MaxWalkSpeed` 1000, `DashSpeedMultiplier` **2.0**, `SpeedBoostMultiplier` 1.5 |
| `BP_Unit` → `InkTank` | `RefillPerSecond` **0.10**, `RefillDelayAfterSpend` 0.5 |
| `BP_Unit` → `CharMoveComp` 바닥 배율 | `OwnFloorMultiplier` 1.5, `EnemyFloorMultiplier` 0.5, `EnemyFloorDashMultiplier` 1.0 |
| `BP_GameMode` | `MatchDuration` **180**, `CountdownDuration` 0, `ItemSpawnInterval` 3 |
| `Lvl_Stage`의 `BP_Balloon` **9개 전부** | `BurstPaintball` **`DA_Paintball_BalloonBurst`**, `BurstCount` **8**, `BurstSpeed` 200, `MaxHealth` 20 |
| `DA_Item_ChocolateFountain` | `Lifetime` 3, `BurstCount` 4, `BurstInterval` 1.0, `BurstGrowth` 1.4 |
| `BP_ItemPickup` | `PillarTemplate` = `NS_ItemPillar`, `PillarSeconds` 0, `BoxSparkleTemplate` = `NS_ItemBoxSparkle`, `BoxSparkleHeight` 60 |
| `BP_Unit` 카메라 | `ViewPitchMin` **-45**, `ViewPitchMax` **45** |
| `BP_Unit` 무기 | `PaintWeapon.Profile` = `DA_Weapon_Fan_T`, `SecondaryWeapon.Profile` = `DA_Weapon_Sniper_T` |
| `DA_Weapon_Fan_T` | `FireMode` **Automatic**, `ShotsPerSecond` 4, `InkCostPercent` 5, 추종탄 3발 / 0.03초 |
| `DA_Paintball_Heavy_Trail_T` | `TrailRadius` **700**, `TrailSpacing` 40, `MaxTrailSplats` 64, `DropAfter` 0.1, `DropGravityScale` 4, `bTrailFirstRayDown` true, 스턴 **0.5** / 슈퍼아머 **2** |
| `DA_Paintball_Fan_Painter_T` | `TrailRadius` **700**, `bHideMesh` true, `TrailSpacing` 60, `MaxTrailSplats` 24, hitPower·stunDuration 0 |
| `DA_Weapon_Sniper_T` | `ChargeTime` **1.5**, `MinChargeToFire` **0.333**, `InkCostPercent` **0**, `ChargeInkPercentPerTick` **10** / `ChargeInkTickSeconds` **0.5**, `Range` **2500**, `RangeHalvingSeconds` **0.5**, `VolleySpacing` **170**, `MaxVolleyShots` **14**, `VolleySpeed` 1800, `VolleyDropLead` 270 |
| `DA_Weapon_Sniper_T.Impact` | hitPower 100, `StunDuration` **1.5**, `StunSuperArmorDuration` **3**, `FullChargeStunSeconds` **3.0**, `FullChargeThreshold` **0.99** |
| `DA_Paintball_SniperVolley_T` | `DropGravityScale` 18, `bHideMesh` **false**(보이게 두기로 함) |
| `BP_Bee` | `TrailFX` = `NS_ArrowTrail_Magic`(main 것), `BodyFX` = `NS_BeeSkull` |
| `UnitMovementComponent.h` | `DoJump` 선언과 `DashJumpZVelocity`가 있어야 한다(`.cpp`가 쓴다) |

### 실제로 있었던 여섯 건

| 무엇 | 언제 | 어떻게 드러났나 |
|---|---|---|
| `DA_Unit_Mint/Choco`, `ABP_Unit` | 09-11 갈래 머지 | PIE에서 캐릭터가 안 보임 |
| `AUnit::ApplyViewPitchLimits` | `b69ecd3f` game-effect 병합 | 시야 상하 제한이 없어짐 |
| `UnitAnimInstance`의 `bIsAiming` / `bWeaponPoseHeld` | `fefb995` main → QA | 무기 자세가 안 나옴 |
| `UUnitMovementComponent::DoJump` 선언 | `384e270` main → QA | **컴파일 에러** |
| 점프대 속도 · 아이템 스폰 모드 · 스턴 큐브 | `bdd682a` Sound → QA | 큐브가 사라진 것으로 발견 |
| 시야각 45 · 꿀벌 해골 · 아이템 스폰 모드 · 스턴 큐브 | `59215e2` main → QA | 충돌을 전부 main 쪽으로 풀어서 |

`59215e2`는 **충돌 파일이 10개**였고 그중 `.uasset`/`.umap` 5개가 main 쪽으로 통째로 갔다.
C++ 5개는 정상적으로 합쳐졌다(양쪽 변경이 다 들어옴). 바이너리 애셋은 섞을 수 없으므로
**충돌하면 반드시 한쪽이 통째로 죽는다** — 이것이 이 목록이 계속 늘어나는 이유다.

충돌한 파일만 뽑는 법:

```bash
BASE=$(git merge-base <내쪽> <상대쪽>)
comm -12 <(git diff --name-only $BASE <내쪽> | sort) <(git diff --name-only $BASE <상대쪽> | sort)
```

여기에 걸린 `.uasset`만 하나씩 열어 값을 확인하면 된다. 전체 diff를 보는 것보다 훨씬 빠르다.

---

## 이번에 알아낸 함정

`docs/Traps.md`와 `docs/UnrealMcp.md`에 아직 없는 것들이다.

### `TMap` 프로퍼티는 일부만 쓰면 나머지가 날아간다

`ObjectTools.set_properties`로 `DA_SoundBank.Events`에 한 항목만 쓰면 **나머지 항목이 전부
사라진다.** 스크래치 사본에서 39개가 1개로 줄었다. 전체 맵을 통째로 다시 써야 한다.
배열 규칙(한 번에 하나씩 늘리기)과 같은 자리에 둘 내용이다.

키 **개수가 같은 채로 키만 바꾸면** 거부된다 —
`keys swapped without size change; changes are ambiguous`.
빈 맵 `{}`을 먼저 쓰고 다시 채우면 통과한다.

반면 **중첩 구조체는 부분 쓰기가 된다.** `Deposit.brushProfile` 하나만 바꿔도
`splatVolume`·`hitPower` 등은 보존된다.

맵의 키 표기는 키 타입에 따라 다르다. 게임플레이 태그 키는 `(TagName="Audio.Weapon.Fire")`
꼴이고(`DA_SoundBank.Events`), 열거형 키는 **첫 글자가 소문자인 열거자 이름**이다
(`DA_Unit_*.ActionFeedback`의 `fire`, `dash`, `charge`, `stun`). 쓰기 전에 한 번 읽어
그 에셋이 쓰는 표기를 확인할 것.

### 발사 원점은 조준선이고, 보정을 받는 쪽과 못 받는 쪽이 있다

`PaintAim::FireOrigin`이 발사 지점을 **시선 위, 폰 깊이**로 잡는다. 총구 소켓은 애니메이션을
타므로 쓰지 않는다(`CLAUDE.md`에도 있다). 눈에 그렇게 안 보이는 이유는 **시각 보정** 때문이다:

- 샷건: `UPaintGunProfile::Launch`가 `VisualOffset = Shot.VisualMuzzle - Shot.Muzzle`을 넘겨
  공의 메시를 총구에서 출발시킨 뒤 `VisualMergeSeconds`(0.12초) 동안 실제 경로로 합류시킨다.
- **차지샷 볼리: 이 보정을 안 받는다.** `APaintVolley::FireNext`가 `Launch`에 `VisualOffset`을
  넘기지 않아 기본값 `ZeroVector`가 쓰인다. `FPaintVolleyParams`에 시각 원점 필드 자체가 없다.
  구조체 주석은 "하늘이 아니라 총에서 나간다"고 적혀 있지만 실제로 받는 `Origin`은 조준선 원점이다.
- `PaintTrail`은 **탄이 아예 없다.** 조준선 위 점에서 수직 아래로 광선을 쏴 바닥을 즉시 도장하므로
  합류시킬 메시가 없고, 조준선이 바닥에 그대로 드러난다.

고치려면 `FPaintVolleyParams`에 `VisualOrigin`을 더하고 `SpawnTrailVolley`가 `Context.VisualMuzzle`을
받아 채운 뒤 `FireNext`가 `VisualOffset`을 계산해 넘기면 된다. 복제 구조체라 레이아웃 변경이므로
리빌드 후 에디터 재시작이 필요하다. **아직 안 했다.**

### `bSkipTrailWhenVolleying`은 버그가 아니다

`UPaintSniperProfile`의 주석이 이유를 적어 뒀다 — 볼리를 쓰면 줄무늬가 탄이 닿는 순서대로
생기는데, 즉발 도포(`PaintTrail`)를 같이 켜면 이미 칠해진 자리에 탄이 도착해 그 연출이 안 보인다.
끄면 조준선이 바닥에 그대로 그려지는 부작용도 따라온다. **기본값 true를 유지할 것.**

### 낙하 중 전진 거리

탄은 꺾인 뒤에도 바닥에 닿을 때까지 앞으로 나아간다. 이 몫을 `DropLead`로 미리 빼 주지 않으면
목표를 그만큼 지나친다. **`DropLead`보다 가까운 목표는 전부 즉시 꺾여 한자리에 뭉친다.**

`DA_Paintball_Heavy_Trail_T` 기준(`DropGravityScale` 4, `MuzzleSpeed` 2400, 발사 높이 약 150cm):

```
낙하 시간 = √(2 × 150 / (980 × 4)) = 0.277초
그 사이 전진 = 0.277 × 2400 = 664cm      → PelletDropLead 660
```

| `DropGravityScale` | 낙하 중 전진 | 최소 착탄 거리 |
|---|---|---|
| 4 (지금) | 664cm | 약 7m |
| 16 | 332cm | 약 3.3m |
| 30 | 242cm | 약 2.4m |

더 가까이 깔려면 `DropGravityScale`을 올리고 `Lead`를 같이 줄여야 한다. 올리면 탄이 더 뚝
떨어지는 느낌이 된다.

### 헤드리스 테스트 이름

`IMPLEMENT_SIMPLE_AUTOMATION_TEST`에 적힌 문자열을 그대로 써야 한다. 사운드뱅크 테스트는
`MintChoco.Audio.Bank`다. 파일 이름(`SoundBankTest.cpp`)에서 유추하면 틀린다.

```
No automation tests matched '...'   ← 이름이 틀렸다는 뜻
```

---

## 커밋

| 커밋 | 내용 |
|---|---|
| `4b641c7` | 09-11 갈래 머지로 되돌아간 유닛 데이터 애셋 복구 (`DA_Unit_Mint/Choco`, `ABP_Unit` 삭제) |
| `39cf72e` | `origin/main` 머지 (아이템 UI 에셋, `WBP_GameHUD`) |
| `7cab709` | 밸런스 수정 및 사운드 수정 1차 |
| `875b8ac` | `main` → `QA` 머지 |
| `2f20001` | 밸런스 수정 2차 (펠릿 계단 낙하) |
| `eeb79a7` / `64cd5c0` | 스턴 이펙트 추가, 수정 + 테스트용 스턴 큐브 |
| `8e26ae8` | 시야 피치 복구, 초콜릿 분수, 점프 밸런스, 아이템 스폰 주기 + 빛 기둥 |
| `81f3848` | 아이템 파괴 연출·획득 연출 되돌리기 |
| `384e270` | `main` → `QA` 머지. **`DoJump` 선언이 떨어져 나갔다** |
| `bdd682a` | `Sound` → `QA` 머지. **점프대·스폰 모드·스턴 큐브가 되돌아갔다** |

### `4b641c7`에서 있었던 일 — 되풀이하면 안 되는 것

`55a54e0 Merge branch 'main'`이 **`origin/main`에 한 번도 올라간 적 없는 09-11 갈래(`2cc77d8`)**를
QA에 합치면서 세 파일을 4일 전 상태로 되돌렸다. 되돌아간 파일은 `DA_Unit_Mint`, `DA_Unit_Choco`,
`ABP_Unit` 셋뿐이었다.

- `DA_Unit_*`는 LFS 포인터 안에 충돌 마커가 그대로 커밋된 상태였다. `git status`는 깨끗하게
  보이는데(클린 필터가 왕복해서) 애셋으로는 못 읽는 파일이었다 —
  `Invalid value for PACKAGE_FILE_TAG at start of file`.
- 그 결과 `Mesh`·`AnimClass`가 전부 `None`이 되어 `Unit::ApplyUnitData`의 `SetSkeletalMesh`가
  아무것도 붙이지 못했고, PIE에서 캐릭터가 보이지 않았다.
- 충돌 **양쪽 다 쓸 수 없었다.** HEAD 쪽은 `/Game/Assets/ClaudeIsGod/SKM_Mint_RigTest` 같은
  저장소에 없는 경로를 가리키는 리그 테스트 스냅샷이었다. 올바른 내용은 머지 직전 QA 커밋
  (`020ad71`)에 있었다.

**교훈**: 오래된 로컬 브랜치를 머지하기 전에 `git diff --name-status <좋은쪽> <머지결과>`로
무엇이 되돌아갔는지 본다. LFS 포인터 파일은 `<<<<<<<` 를 직접 grep해야 잡힌다.

---

## 아직 안 한 것

- 차지샷 볼리에 `VisualOffset` 전달 (위 [발사 원점](#발사-원점은-조준선이고-보정을-받는-쪽과-못-받는-쪽이-있다) 참고)
- `MintChoco.Audio.Bank`의 빈 슬롯 20개
- `USoundConcurrency` 에셋 — 프로젝트에 하나도 없다. MCP로는 만들 수 없으니 에디터에서
  직접 만들어야 한다.
- `_Test` 사본을 원본으로 되돌릴지 결정. 되돌린다면 `_T`의 값을 원본에 옮기고 `BP_Unit`의
  두 줄을 원래대로 돌리면 된다.
- **PIE 검증.** 밸런스 세 단계가 전부 들어갔지만 아직 눈으로 본 것은 없다. 특히 볼 것:
  차지샷 충전 중 잉크가 0.5초마다 10%씩 빠지는지, 덜 충전한 샷이 짧게 나가는지,
  샷건 궤적이 위를 보고 쏴도 이어지는지, 상대 색 위에서 보드가 느려지는지
