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

### 차지샷 볼리 간격

- **지금 값**: `DA_Weapon_Sniper_T.VolleySpacing` 300 → **100cm**.
  폭탄 자국 폭이 187cm라 간격 100이면 겹쳐서 연속 띠가 된다. 첫 폭탄이 총구 100cm 앞이라
  발밑부터 덮인다. 사거리 3800에서 37발, `MaxVolleyShots` 64에 안 걸린다.
  전부 깔리는 데 37 × `VolleyInterval` 0.04 = 약 1.5초.
- `bSkipTrailWhenVolleying`은 **true 그대로 뒀다**. 아래 [연출 보호 장치](#bskiptrailwhenvolleying-는-버그가-아니다) 참고.

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

지점은 **9개다.** 10개가 아니다.

### 빛 기둥 (`AItemPickup::Pillar`)

| | |
|---|---|
| 어디에 | `AItemPickup`에 `UNiagaraComponent Pillar` + `PillarTemplate` + `PillarSeconds` |
| 무엇을 | 픽업이 **활성이 되는 순간** 켜고 `PillarSeconds` 뒤에 끈다. 누가 가져가면 즉시 끈다 |
| 대체한 것 | 없음(새 기능) |
| 값 | `PillarSeconds` 5초, `BP_ItemPickup.PillarTemplate` = `NS_ItemPillar` |

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
| `Lvl_Stage`의 `BP_ItemSpawnPoint` 9개 | `SpawnMode` **Standalone**, `RespawnDelay` **10초** (되돌아가면 Shared / 3초) |
| `Lvl_Stage`의 `TestStunZone` | 있어야 한다. (0, 0, 409.5), 반경 400 |
| `BP_Unit` → `CharMoveComp` | `GravityScale` 2.0, `JumpZVelocity` 660, `DashJumpZVelocity` 660, `AirControl` 0.15 |
| `DA_Item_ChocolateFountain` | `Lifetime` 3, `BurstCount` 4, `BurstInterval` 1.0, `BurstGrowth` 1.4 |
| `BP_ItemPickup` | `PillarTemplate` = `NS_ItemPillar`, `PillarSeconds` 5 |
| `UnitMovementComponent.h` | `DoJump` 선언과 `DashJumpZVelocity`가 있어야 한다(`.cpp`가 쓴다) |

### 실제로 있었던 다섯 건

| 무엇 | 언제 | 어떻게 드러났나 |
|---|---|---|
| `DA_Unit_Mint/Choco`, `ABP_Unit` | 09-11 갈래 머지 | PIE에서 캐릭터가 안 보임 |
| `AUnit::ApplyViewPitchLimits` | `b69ecd3f` game-effect 병합 | 시야 상하 제한이 없어짐 |
| `UnitAnimInstance`의 `bIsAiming` / `bWeaponPoseHeld` | `fefb995` main → QA | 무기 자세가 안 나옴 |
| `UUnitMovementComponent::DoJump` 선언 | `384e270` main → QA | **컴파일 에러** |
| 점프대 속도 · 아이템 스폰 모드 · 스턴 큐브 | `bdd682a` Sound → QA | 큐브가 사라진 것으로 발견 |

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
