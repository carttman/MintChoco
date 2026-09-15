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
  `FX` = `NS_Sparkling_Animate_2`, `FXSocket` = `head`, `FXOffset` = `(0, 0, 30)`.
  소켓 기준 오프셋을 살려야 머리 위로 뜨므로 `EAttachLocation::KeepRelativeOffset`을 쓴다.

캐릭터 메시에 **머리 전용 소켓은 없다.** `SKM_Character_Mint`의 커스텀 소켓은 `Board`, `InkBottle`,
`Gun`뿐이라 `head` **본**에 직접 붙였다. 높이는 `FXOffset`으로 맞춘다.

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
