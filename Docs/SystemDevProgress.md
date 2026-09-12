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
| **2** | **꿀풍선 조준 모드** | **다음 작업** |
| 7 | 샷건 퍼짐 · 점유율 1 % | 대기 (`MaxRange` 항목은 2단 중력으로 대체됨) |
| 9 | KO 판정 (70 % 5초) | 대기 |
| 5 | 차지샷 순차 낙하 | 대기 |
| 6 | 꿀벌 낙하 방식 전환 | 대기 |
| 3 | 히어로 랜딩 (가장 위험) | 대기 |

## 확정된 설계 결정

- **조준 제한 시간 10초.** 디저트 폭격 확정, 꿀풍선도 동일. 히어로 랜딩은 기존 `Duration 6`초 유지.
- **조준 취소 입력 없음.** 빠져나가는 길은 10초 만료뿐.
- **만료 시 아이템은 잃는다.** 활성화 시점에 슬롯이 비므로 추가 코드가 없다.
- **차지샷 이중 도포 허용.** 기존 `Trail` 도포를 끄지 않는다. 5단계 이후 P0의 "풀충전 5 %"를 다시 재야 한다.

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
- **1단계가 그대로 따라 할 본보기**: `UGA_DessertBombardment`. 즉발 → 지속형 전환, 전용 GE 클래스
  (`UGE_DessertBombardment`, 스택 분리용), 미리보기 BP 는 프로필의 `TSubclassOf<AActor>` 로 뺐다.

## 미커밋 변경 (HEAD = 13b620d)

신규 파일: `Source/MintChoco/Items/ItemAimAbility.{h,cpp}`, `Content/Blueprints/Items/BP_BombardmentAimLine.uasset`

수정: `Unit.cpp`, `ItemSlotComponent.{h,cpp}`, `ItemGameplayTags.{h,cpp}`, `ItemGameplayEffect.h`,
`DessertBombardment{Ability.h,Ability.cpp,Profile.h}`, `ItemProfileAssetTest.cpp`,
`PaintProjectile.{h,cpp}`, `PaintballProfile.h`,
`BP_Unit`, `DA_Item_DessertBombardment`, `DA_Weapon_Fan`, `DA_Weapon_Shotgun`,
`DA_Scatter_Spinner`, `DA_Paintball_Heavy_Trail`

## 확인하지 않은 것

- **0 · 1단계는 PIE 에서 검증되지 않았다.** 빌드와 자동화 테스트(40/40)만 통과했다.
  조준 → 좌클릭 → 폭격, 10초 만료, 조준선이 실제 폭격 범위와 일치하는지는 아직 눈으로 안 봤다.
- 샷건 2단 중력(`DropAfter 0.2` / `DropGravityScale 4` / `GravityScale 0.05`)의 손맛도 미검증.

## 함정 (실제로 시간을 날린 것들)

- **"샷건"은 `DA_Weapon_Shotgun` 이 아니다.** `BP_Unit` 의 `PaintWeapon.Profile` 은 `DA_Weapon_Fan` 이고,
  `DA_Weapon_Shotgun` 은 어디에서도 참조되지 않는 고아 에셋이다. 무기 값을 바꿀 때는 Fan 을 봐야 한다.
- `EPaintFireMode::Single` 은 원래 연사 간격이 없었다(클릭하는 만큼 나갔다). 4단계에서 `LastShotTime` 으로
  두 번의 당김 사이 최소 간격을 넣었다. 그래서 `ShotsPerSecond` 가 이제 Single 에서도 의미를 가진다.
- 조준 수렴(`PaintGunProfile::Fire`)은 총구 위치만 보정하고 **중력은 보정하지 않는다.** 탄이 크로스헤어보다
  낙차만큼 아래에 맞는 이유다. 샷건은 직진 구간 중력을 0.05 로 낮춰 이를 피해 갔다.
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
