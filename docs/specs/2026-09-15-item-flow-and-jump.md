# 아이템 흐름·연출과 점프 밸런스 — 설계

작성 2026-09-15, 브랜치 `QA`. 네 건을 한 문서에 담되 **서로 독립적**으로 만들어,
어느 하나가 막혀도 나머지가 진행되도록 한다.

구현 계획(순서, 검증, 되돌리기)은 별도 문서로 나간다. 이 문서는 **무엇을 왜 그렇게 만드는지**만 말한다.

---

## 조사에서 드러난 전제 수정

계획을 세우기 전에 확인한 것들이다. 요청 내용과 실제 코드가 어긋나는 지점이 셋 있었다.

| 요청 | 실제 |
|---|---|
| "아이템 소환 장소가 10개" | **9개**다. 전부 `EItemSpawnMode::Shared` |
| "시작하자마자 전부 생성 + 개별 리스폰" | `EItemSpawnMode::Standalone`이 **이미 그 동작을 한다**. 모드만 바꾸면 된다 |
| "초콜릿 분수는 그 시간 동안 지속적으로 칠해진다" | **한 번만** 칠한다. 설치 순간 `APaintBurst` 하나, 이후 돔은 서 있기만 한다 |

그리고 점프대는 C++ `AJumpPad`가 아니다. `Lvl_Stage`에 놓인 것은
`/Game/LevelPrototyping/Interactable/JumpPad/BP_JumpPad` — **순수 블루프린트**이고,
발사량은 `velocity` 벡터 프로퍼티(현재 `Z = 1100`)다. C++ `AJumpPad`는 레벨에 0개다.

**따라서 점프대의 발사 로직은 수정 대상에서 뺀다.** 값만 조정한다. 로직을 바꿔야 할 일이
생기면 블루프린트 작업이므로 개발자에게 넘긴다.

---

## ① 아이템 스폰 주기

### 바꿀 것

9개 지점을 전부 `Standalone`으로 돌리고, 게임모드의 주기 스폰을 끈다.

```
지금   게임모드가 15초마다 빈 지점 하나를 골라 놓는다 (ItemSpawnInterval 15)
       → 9개가 동시에 차는 일이 없다

바꿈   경기 시작에 9개 전부 놓인다
       가져가면 그 지점만 개별로 10초 뒤 다시 놓는다
```

`AItemSpawnPoint::SpawnMode`를 `Standalone`, `RespawnDelay`를 `10`으로 바꾸면 끝이다.
`StartItemSpawning()`은 `Shared` 지점만 모으므로 목록이 비고, 주기 타이머는 스스로 꺼진다.
**게임모드 코드를 고칠 필요가 없다.**

### 빛 기둥

새로 만드는 유일한 부분이다.

- **어디에**: `AItemPickup`에 `UNiagaraComponent`를 하나 더 두고, 픽업이 활성 상태가 될 때
  켜서 `PillarSeconds`(기본 5) 뒤에 끈다. 픽업 자신이 들고 끄므로 별도 액터가 필요 없다.
- **왜 픽업에 두나**: 빛 기둥은 "여기 아이템이 새로 생겼다"는 신호다. 픽업의 수명과 정확히
  묶여 있어야 하고, 픽업이 사라지면 같이 사라져야 한다.
- 기존 `Laser` 컴포넌트(예고용 스태틱 메시)와는 **다른 것**이다. 그쪽은 나타나기 *전* 예고,
  이쪽은 나타난 *후* 표시다.
- 에셋: 새 Niagara 시스템 `NS_ItemPillar`. 기둥은 단순한 원통 스프라이트/메시 + 위로 흐르는
  빛이면 충분하다.

빛 기둥 색은 **하나로 통일한다.** 아이템별로 나눌 수가 없다 — 놓이는 것은 무작위 상자이고,
무엇이 들었는지는 **먹은 뒤에야** 정해져 보인다. 기둥 색이 내용물을 미리 알려주면 그 재미가 없어진다.

---

## ④ 초콜릿 분수 — 넘치는 도포

### 지금

```
설치 순간 APaintBurst 한 번 (반경 300cm)
돔이 5초간 서 있는다
```

### 바꿈

```
t=0s   1.0배 = 300cm      설치와 동시에
t=1s   1.2배 = 360cm
t=2s   1.44배 = 432cm
t=3s   1.73배 = 519cm     여기서 끝, 돔도 사라진다
```

1초마다 직전의 1.2배. 세 번 반복하면 1.728배로 요청하신 "1.7배 정도"와 맞는다.

- **어디에**: `UChocolateFountainProfile`에 `BurstInterval`(1초), `BurstGrowth`(1.2),
  `BurstCount`(4, 설치 순간 포함)를 더한다. `AChocolateFountainAbility`가 지금은
  `APaintBurst::Spawn`을 한 번 부르는데, 타이머로 `BurstCount`번 부르게 바꾼다.
- `Lifetime`을 5초 → **3초**로 바꾼다. 마지막 도포와 돔이 같이 끝나야 한다.
- 반경은 `MakeBurst`가 `bBurstMatchesRadius`로 속도를 계산하므로, **배율을 반경에 곱하면
  도포 범위가 따라온다**. 새 계산이 필요 없다.
**돔 크기는 그대로 둔다.** 커지는 것은 바닥 도포뿐이다. 돔은 탄을 막는 벽이라 크기가 변하면
전투 판정이 흔들린다.

가장 작은 작업이다. C++ 30줄 안쪽 + 값 세 개.

---

## ⑤ 점프 밸런스

### 지금 (실측)

```
JumpZVelocity 420,  GravityScale 1 (980 cm/s²),  AirControl 0.05
MaxWalkSpeed 1000,  DashSpeedMultiplier 1.7 (= 1700)
BP_JumpPad.velocity.Z 1100
```

| | 최고 높이 | 체공 | 수평 도달 |
|---|---|---|---|
| 그냥 점프 | 90cm | 0.86초 | 8.6m |
| 대쉬 점프 | **90cm** | **0.86초** | 14.6m |
| 점프대 | 617cm | 2.24초 | — |

**그냥 점프와 대쉬 점프의 높이·체공이 완전히 같다.** 역할이 안 나뉜 원인이 이것이다.
차이는 수평 속도뿐이고, 그나마 `AirControl` 0.05라 공중에서 방향도 못 바꾼다.

### 바꿈 — 중력을 올려 체공을 깎고, 높이로 역할을 나눈다

공식은 두 개뿐이다. `높이 = v² / 2g`, `체공 = 2v / g`.
**중력만 올리면 높이와 체공이 같이 줄고, 속도를 같이 올리면 높이는 되살리면서 체공만 준다.**

`GravityScale`을 **2.0**(1960 cm/s²)으로 올린 뒤 각각의 발사 속도를 다시 잡는다.

| | v | 최고 높이 | 체공 | 수평 도달 | 역할 |
|---|---|---|---|---|---|
| 그냥 점프 | 660 | 111cm | 0.67초 | 6.7m | 턱 넘기. 지금보다 높지만 훨씬 빠르다 |
| 대쉬 점프 | 520 | **69cm** | 0.53초 | **9.0m** | 낮게, 멀리. 이동기 |
| 점프대 | 1750 | **781cm** | 1.79초 | — | 높은 곳 오르기 |

- 셋 다 **체공이 줄었다**: 0.86 → 0.67 / 0.53, 2.24 → 1.79초.
- 그냥 점프는 **높이가 올랐는데 체공은 22% 줄었다**. 요청하신 "높이와 속도는 증가, 체공은 감소"다.
- 대쉬 점프는 일부러 **더 낮게** 만들어 "멀리 가는 대신 높이 못 간다"를 분명히 했다.
- 점프대는 617 → 781cm로 올려 다른 둘과 격차를 키웠다.

### 대쉬 점프를 따로 만들려면 C++이 필요하다

지금은 대쉬 중이든 아니든 `JumpZVelocity` 하나를 쓴다. 분리하려면
`UUnitMovementComponent`에 `DashJumpZVelocity`를 더하고, 점프 순간 대쉬 중이면 그 값을
쓰도록 해야 한다. 이동 예측을 타는 값이라 **압축 플래그가 아니라 이미 복제되는 대쉬 상태를
보고 고르는** 방식이어야 한다.

### `AirControl`을 0.05 → 0.15로 올린다

0.05는 사실상 0이다. 대쉬 점프를 "멀리 가는 이동기"로 만들려면 공중에서 약간은 틀 수 있어야
쓸모가 생긴다. 다만 값이 커질수록 점프대 착지 지점을 공중에서 조준해 버려 점프대의 역할이
흐려진다. 0.15가 상한이라고 보고, PIE에서 점프대 쪽을 함께 확인한다.

### `BP_JumpPad` 발사 방식 — 확인 완료

그래프를 확인했다(`/Game/LevelPrototyping/Interactable/JumpPad/BP_JumpPad`).

```
Event ActorBeginOverlap
  → Actor Has Tag (Exclusion Tag = "Dead") → Branch
  → Cast To Character → Launch Character
       Launch Velocity = Velocity 변수 (0, 0, 1100)
       XYOverride  = 꺼짐   → 수평 속도는 그대로 둔다(더하기)
       ZOverride   = 켜짐   → 수직 속도는 덮어쓴다
```

**C++ `AJumpPad`와 같은 규칙이다.** Z를 덮어쓰므로 밟기 직전에 올라가던 중이든 떨어지던
중이든 결과가 같고, 달려온 수평 기세는 살아남는다. 따라서 위 수치표의 계산이 그대로 성립하고,
**`velocity.Z` 값만 바꾸면 된다.** 블루프린트 그래프를 고칠 일이 없다.

컨스트럭션 스크립트는 색과 라이트만 다루므로 밸런싱과 무관하다.

### 미결

- 중력 2.0은 **낙하 전체**에 걸린다. 점프대에서 떨어질 때도 두 배로 빨라지므로,
  히어로 랜딩(공중 아이템)의 체감이 달라질 수 있다. PIE 확인이 필요하다.

---

## ⑥ 시야 피치 제한 — 사라졌다. 복구한다

**결론: 연결이 끊긴 게 아니라 코드가 삭제됐다.** 지금 `Source/` 어디에도 `ViewPitch`라는
글자가 없다(`UnitAnimInstance`의 `AimPitch`는 다른 것이다).

### 원래 있던 것

커밋 `f360cba` (2026-09-10 18:04 "기능추가_1")이 `AUnit`에 넣었다:

```cpp
// Unit.h
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
    meta = (ClampMin = "-89.9", ClampMax = "0", ForceUnits = "deg"))
float ViewPitchMin = -45.0f;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
    meta = (ClampMin = "0", ClampMax = "89.9", ForceUnits = "deg"))
float ViewPitchMax = 60.0f;

void ApplyViewPitchLimits();
```

```cpp
// Unit.cpp — SetupPlayerInputComponent 끝에서 호출
void AUnit::ApplyViewPitchLimits()
{
    const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
    if (APlayerCameraManager* const CameraManager =
            PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr)
    {
        CameraManager->ViewPitchMin = ViewPitchMin;
        CameraManager->ViewPitchMax = ViewPitchMax;
    }
}
```

클램프는 **소유 클라이언트의 카메라 매니저만** 건다. 서버는 이미 클램프된 회전을 `ServerMove`로
받으므로 따로 걸 필요가 없다. `SetupPlayerInputComponent`에서 부르는 이유는 그곳이 로컬 조종
폰이 확정되는 유일한 지점이고, 리스폰하면 다시 불려 새 카메라 매니저에도 자동으로 걸리기
때문이다.

### 어떻게 사라졌나

`f360cba`는 HEAD의 조상이 **맞다.** 그런데 이후 머지들을 거치며 이 코드가 있었다 없었다를
반복하다가 마지막에 0이 됐다. 마지막으로 떨어져 나간 지점은 `b69ecd3f`
(2026-09-13 20:17, "game-effect 병합", 부모 2개)다.

`git log -S`로는 안 잡힌다. **머지가 떨어뜨린 코드는 `-S`의 기본 히스토리 단순화에서 빠지기
때문이다.** 09-11 갈래가 `DA_Unit_*`를 되돌린 것과 같은 종류의 사고다.

### 복구 방법

위 코드를 그대로 되살린다. 세 곳뿐이다 — `Unit.h`의 값 둘과 선언, `Unit.cpp`의 함수와
`#include "Camera/PlayerCameraManager.h"`, `SetupPlayerInputComponent` 끝의 호출 한 줄.

값(-45 / +60)은 당시 그대로 되살리고, 실제 조준감은 PIE에서 보고 조정한다.

> ⑤에서 중력을 2배로 올리므로 위를 보는 각도(+60도)의 체감이 달라질 수 있다.
> **⑤와 ⑥은 함께 확인하는 것이 좋다.**

---

## 작업 순서

위험이 낮고 독립적인 것부터.

| 단계 | 내용 | 무엇이 필요한가 | 빌드 |
|---|---|---|---|
| 1 | ⑥ 시야 피치 복구 | 지워진 코드 되살리기. 세 곳 | 필요 |
| 2 | ④ 초콜릿 분수 | C++ 소폭 + 값 3개 | 필요 |
| 3 | ⑤ 점프 밸런스 | C++ 소폭(대쉬 점프 분리) + 값 4개 | 필요 |
| 4 | ① 스폰 주기 + 빛 기둥 | 값 변경 + `AItemPickup` C++ + Niagara 1종 | 필요 |

1~3은 한 번의 빌드로 묶을 수 있다. ⑥과 ⑤는 조준·이동 체감이 겹치므로 **함께 확인하는 편이
낫다.**

---

## 결정된 것

| | 결정 |
|---|---|
| ① 스폰 지점 | **9개 그대로**. 전부 `Standalone`, `RespawnDelay` 10초 |
| ① 빛 기둥 색 | **통일**. 상자는 내용물을 모르므로 나눌 수가 없다 |
| ④ 돔 크기 | **그대로**. 커지는 것은 바닥 도포뿐 |
| ④ 배율·시간 | 1.0 → 1.2 → 1.44 → 1.73배, 1초 간격, `Lifetime` 5 → 3초 |
| ⑤ 방향 | **역할 분리**. 중력 2배로 체공을 깎고 높이로 역할을 나눈다 |
| ⑤ `AirControl` | 0.05 → **0.15** |
| ⑤ 점프대 | `velocity.Z` 값만 변경. 그래프는 안 고친다(Z 덮어쓰기 확인됨) |
| ⑥ 시야 피치 | **삭제된 코드 복구**. 값은 −45 / +60으로 되살린다 |

## 남은 확인 (구현 중 PIE에서)

- ⑤ 중력 2배가 히어로 랜딩 등 **공중 아이템 체감**에 주는 영향
- ⑤ `AirControl` 0.15가 점프대 착지 조준을 너무 쉽게 만드는지
- ⑥ 중력 변경 후 위쪽 시야 +60도가 여전히 적당한지
