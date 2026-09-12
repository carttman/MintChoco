# 조준 모드 개발 — 진행 상황

전체 계획은 `Docs/SystemDevPlan.html`(10단계, 착수 순서와 의존 관계)에 있다.
이 문서는 **지금 어디까지 했고 다음이 무엇인지**만 적는다. 작업이 끝날 때마다 갱신한다.

최종 갱신: 2026-09-12 · 브랜치 QA

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
| **9** | **KO 판정 (70 % 5초)** | **다음 작업** |
| 5 | 차지샷 순차 낙하 | 대기 |
| 6 | 꿀벌 낙하 방식 전환 | 대기 |
| 3 | 히어로 랜딩 (가장 위험) | 대기 |

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

## 확인하지 않은 것

- **0 · 1 · 2단계는 PIE 에서 검증되지 않았다.** 빌드와 자동화 테스트(40/40)만 통과했다.
  조준 → 좌클릭 → 발동, 10초 만료, 표시가 실제 결과와 일치하는지는 아직 눈으로 안 봤다.
- 꿀풍선 궤적은 `UGameplayStatics::PredictProjectilePath` 로 그린다. 발사 지점·속도는
  `UGA_HoneyBalloon::ComputeThrow` 하나에서 나오므로 미리보기와 실제 투척이 어긋날 수 없다.
  다만 **확정 순간의 서버 시선**으로 던지므로, 클릭 직전에 크게 돌리면 RTT 만큼 차이가 날 수 있다.
- 샷건 2단 중력(`DropAfter 0.2` / `DropGravityScale 4` / `GravityScale 0.05`)의 손맛도 미검증.
- 7단계 값(팬 30°, 지터 3°, 볼륨 2.4)은 계산으로만 잡았다. 퍼짐 폭은 취향이라 PIE 에서 봐야 한다.

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
