# 점유율 게이지 HUD 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 이미 구현되어 있지만 아무도 쓰지 않는 `UPaintCoverageBarWidget`을 게임 HUD 상단 중앙
타이머 아래에 붙여, 경기 중 팀별 점유율이 실시간으로 보이게 한다.

**Architecture:** 새 위젯을 만들지 않는다. 값(서버 셀 그리드 → `AGameGameState::WorldCoverage` 복제)과
그림(`NativePaint`의 양방향 게이지 + 포센트 라벨)이 모두 이미 있고, 빠진 것은 **위젯 트리에 배치**
하나다. 그래서 코드 작업은 두 가지로 좁힌다: (1) 지금 `NativePaint` 안에 섞여 있어 테스트할 수 없는
칸 계산을 순수 함수로 빼내고 테스트로 고정한다, (2) 라벨이 게이지 폭을 넘지 않게 하는 보정을 그
함수에 함께 담는다. 배치는 `WBP_GameHUD`의 캔버스에 C++ 위젯을 직접 넣는다 — 같은 파일의
`ChargeRing`(`UPaintChargeWidget`)이 이미 그 방식이다.

**Tech Stack:** UE 5.8, C++ (Epic Coding Standard), Slate/UMG, Unreal MCP(`UMGToolSet`, `ObjectTools`),
`UnrealEditor-Cmd` 헤드리스 자동화 테스트.

**Spec:** [docs/superpowers/specs/2026-09-11-coverage-gauge-hud.md](../specs/2026-09-11-coverage-gauge-hud.md)

## Global Constraints

- C++는 Epic Coding Standard: 탭 인덴트, PascalCase, `U`/`A`/`F`/`E` 접두어. `Paint/*`의 주석은 영어다.
- 새 `UPROPERTY`는 모듈 재빌드 + **에디터 재시작** 전까지 `ObjectTools`에 보이지 않는다.
- 외부 빌드는 에디터가 떠 있으면 Live Coding 때문에 실패한다. 빌드 전에 에디터를 닫는다.
- 에디터가 에셋 파일을 잠그므로 `git checkout`으로 콘텐츠를 되돌릴 일이 있으면 에디터를 먼저 닫는다.
- 헤드리스 테스트는 에디터와 같이 띄우지 않는다(포트 8000).
- MCP 세션 도구가 붙지 않으면 PowerShell HTTP 경로를 쓴다:
  `. <scratchpad>/mcp.ps1; Connect-Mcp; Invoke-McpTool -Toolset <full.name> -Tool <BareName> -Arguments @{...}`.
- 검증 맵은 `/Game/Maps/Lvl_Stage`다(`/Game/Sample/Maps/Lvl_Stage` 아님).
- 테스트 명령:
  `UnrealEditor-Cmd.exe D:\GitHub\MintChoco\MintChoco.uproject -ExecCmds="Automation RunTests MintChoco; Quit" -unattended -nullrhi -abslog=D:\GitHub\MintChoco\Saved\Logs\AutoTest.log`
  후 로그에서 `Test Completed. Result=` 확인. 기존 실패 하나(`MintChoco.Paint.Weapons.ProfileAssets`의
  `DA_Scatter_Spinner`)는 이 작업과 무관하다.

## File Structure

- `Source/MintChoco/Paint/PaintCoverageBarWidget.h` — 순수 계산 구조체 `FPaintCoverageBarMath` 선언 추가.
  위젯 클래스 자체는 프로퍼티 하나(`MinVisibleFraction`은 쓰지 않는다 — 스펙이 절대값이다)도 늘리지 않는다.
- `Source/MintChoco/Paint/PaintCoverageBarWidget.cpp` — 계산을 `FPaintCoverageBarMath`로 옮기고
  `NativePaint`가 그것을 호출하게 한다.
- `Source/MintChoco/Tests/PaintCoverageBarTest.cpp` — 새 파일. 순수 계산 테스트.
- `Content/Assets/UI/Widgets/Game/WBP_GameHUD.uasset` — 캔버스에 게이지 위젯 배치(에셋 편집).

---

### Task 1: 게이지 칸 계산을 순수 함수로 빼고 테스트로 고정

지금은 클램프 규칙이 `NativePaint` 안에 있어 테스트가 닿지 않는다. 이 규칙이 스펙의 핵심
("둘이 만나는 지점이 점유율 많은 쪽으로 밀린다")이므로 함수로 꺼내 고정한다.

**Files:**
- Modify: `Source/MintChoco/Paint/PaintCoverageBarWidget.h` (파일 상단, 위젯 클래스 선언 앞)
- Modify: `Source/MintChoco/Paint/PaintCoverageBarWidget.cpp` (`NativePaint` 안의 클램프 두 줄)
- Test: `Source/MintChoco/Tests/PaintCoverageBarTest.cpp` (새 파일)

**Interfaces:**
- Consumes: 없음
- Produces: `FPaintCoverageBarMath::ComputeFills(float RawMint, float RawChoco, float& OutMint, float& OutChoco)`
  — 두 칸의 폭을 0~1 비율로 돌려준다. Task 2는 이 함수를 쓰지 않고 에셋만 만진다.

- [ ] **Step 1: 실패하는 테스트를 쓴다**

`Source/MintChoco/Tests/PaintCoverageBarTest.cpp`를 새로 만든다:

```cpp
#include "Misc/AutomationTest.h"

#include "Paint/PaintCoverageBarWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 게이지 두 칸의 폭. 절대 점유율이므로 합이 1보다 작으면 가운데가 비고, 합이 1을 넘기려 하면
 * 먼저 들어온 민트가 자리를 지키고 초코가 남은 만큼만 차지한다(복제가 따라오는 동안의 과도 상태).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintCoverageBarFillsTest,
	"MintChoco.Paint.Coverage.BarFills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintCoverageBarFillsTest::RunTest(const FString& Parameters)
{
	float Mint = 0.0f;
	float Choco = 0.0f;

	// 둘 다 조금 칠한 상태: 가운데가 남는다.
	FPaintCoverageBarMath::ComputeFills(0.2f, 0.15f, Mint, Choco);
	TestEqual(TEXT("mint keeps its share"), Mint, 0.2f, 1e-4f);
	TestEqual(TEXT("choco keeps its share"), Choco, 0.15f, 1e-4f);
	TestTrue(TEXT("the middle is still bare"), Mint + Choco < 1.0f);

	// 만나는 지점은 점유율이 많은 쪽으로 밀린다: 민트가 더 많으면 경계가 오른쪽으로 간다.
	FPaintCoverageBarMath::ComputeFills(0.6f, 0.4f, Mint, Choco);
	TestEqual(TEXT("the meeting point sits at the mint share"), Mint, 0.6f, 1e-4f);
	TestEqual(TEXT("choco fills the rest"), Choco, 0.4f, 1e-4f);

	// 합이 1을 넘는 과도 상태에서도 두 칸은 겹치지 않는다.
	FPaintCoverageBarMath::ComputeFills(0.7f, 0.5f, Mint, Choco);
	TestEqual(TEXT("mint is unchanged"), Mint, 0.7f, 1e-4f);
	TestEqual(TEXT("choco is cut to the space left"), Choco, 0.3f, 1e-4f);
	TestTrue(TEXT("the two fills never overlap"), Mint + Choco <= 1.0f + 1e-4f);

	// 음수와 1 초과는 잘린다.
	FPaintCoverageBarMath::ComputeFills(-0.5f, 2.0f, Mint, Choco);
	TestEqual(TEXT("a negative share is zero"), Mint, 0.0f, 1e-4f);
	TestEqual(TEXT("an over-full share is one"), Choco, 1.0f, 1e-4f);

	// 아무것도 안 칠한 상태.
	FPaintCoverageBarMath::ComputeFills(0.0f, 0.0f, Mint, Choco);
	TestEqual(TEXT("empty stays empty"), Mint, 0.0f, 1e-4f);
	TestEqual(TEXT("empty stays empty"), Choco, 0.0f, 1e-4f);

	return true;
}

#endif
```

- [ ] **Step 2: 컴파일이 깨지는 것을 확인한다**

에디터를 닫고:

```bash
"/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" MintChocoEditor Win64 Development -Project="D:/GitHub/MintChoco/MintChoco.uproject" -WaitMutex
```

Expected: `'FPaintCoverageBarMath': 정의되지 않은 식별자` 류의 컴파일 실패.

- [ ] **Step 3: 순수 함수를 선언한다**

`PaintCoverageBarWidget.h`의 `struct FPaintCoverage;` 전방 선언 아래, `UCLASS()` 앞에 추가:

```cpp
/** The bar's pure geometry, so the rule that keeps the two fills from overlapping is testable. */
struct MINTCHOCO_API FPaintCoverageBarMath
{
	/**
	 * The two fills as fractions of the bar, 0 to 1. Shares are absolute, so their sum is the
	 * painted part of the world and whatever is left is drawn as the bare middle. Mint keeps its
	 * share and Choco is cut to the space left, so a sum over 1 - which replication can show
	 * briefly while the two values arrive - never draws one fill over the other.
	 */
	static void ComputeFills(float RawMint, float RawChoco, float& OutMint, float& OutChoco);
};
```

- [ ] **Step 4: 구현하고 NativePaint가 그것을 쓰게 한다**

`PaintCoverageBarWidget.cpp`의 인클루드 아래(첫 함수 정의 앞)에 추가:

```cpp
void FPaintCoverageBarMath::ComputeFills(float RawMint, float RawChoco, float& OutMint, float& OutChoco)
{
	OutMint = FMath::Clamp(RawMint, 0.0f, 1.0f);
	// The two fills grow towards each other and must never overlap, or the gap would stop meaning
	// "unpainted". Clamping Choco against the space Mint left keeps the reading honest while
	// replication catches up.
	OutChoco = FMath::Clamp(RawChoco, 0.0f, 1.0f - OutMint);
}
```

같은 파일 `NativePaint`의 두 줄

```cpp
	const float Mint = FMath::Clamp(MintFraction, 0.0f, 1.0f);
	// The two fills grow towards each other and must never overlap, or the gap would stop meaning
	// "unpainted". Clamping Choco against the space Mint left keeps the reading honest while
	// replication catches up.
	const float Choco = FMath::Clamp(ChocoFraction, 0.0f, 1.0f - Mint);
```

를 아래로 교체한다:

```cpp
	float Mint = 0.0f;
	float Choco = 0.0f;
	FPaintCoverageBarMath::ComputeFills(MintFraction, ChocoFraction, Mint, Choco);
```

- [ ] **Step 5: 빌드하고 테스트가 통과하는지 확인한다**

Run: Step 2의 빌드 명령, 이어서 Global Constraints의 테스트 명령.
Expected: 빌드 성공, `MintChoco.Paint.Coverage.BarFills` … `Result=Success`.
기존 실패(`DA_Scatter_Spinner`) 외에 새로 깨지는 테스트가 없어야 한다.

- [ ] **Step 6: 커밋**

```bash
git add Source/MintChoco/Paint/PaintCoverageBarWidget.h Source/MintChoco/Paint/PaintCoverageBarWidget.cpp Source/MintChoco/Tests/PaintCoverageBarTest.cpp
git commit -m "refactor: 점유율 게이지의 칸 계산을 순수 함수로 빼고 테스트로 고정"
```

---

### Task 2: HUD 상단 중앙 타이머 아래에 게이지를 배치

위젯 블루프린트를 새로 만들지 않는다. 같은 캔버스의 `ChargeRing`이 C++ 위젯을 직접 배치한
선례이므로 같은 방법을 쓴다.

**Files:**
- Modify: `Content/Assets/UI/Widgets/Game/WBP_GameHUD.uasset`

**Interfaces:**
- Consumes: `UPaintCoverageBarWidget`(`/Script/MintChoco.PaintCoverageBarWidget`)
- Produces: `WBP_GameHUD:WidgetTree.CoverageBar` — 이름이 정해져 있으므로 나중에
  `UGameHudWidget`에서 `BindWidgetOptional`로 잡을 수도 있다(이번 범위 밖).

배치 값:

| 항목 | 값 | 이유 |
|---|---|---|
| 부모 | `CanvasPanel_39` | HUD의 루트 캔버스 |
| 이름 | `CoverageBar` | |
| 앵커 | min (0.5, 0), max (0.5, 0) | 상단 중앙 고정 |
| 정렬 | (0.5, 0) | 가로 중앙 기준 |
| offsets | left 0, top 34, right 560, bottom 40 | 타이머(0~30px) 아래 4px. 폭 560, 높이 40 |
| ZOrder | 1 | 타이머(0)보다 위, 중앙 카운트다운(10)보다 아래 |

높이 40은 위젯이 라벨(폰트 14 + `LabelPadding` 3)과 게이지(`BarHeight` 18)를 위아래로 쌓기 때문이다.

- [ ] **Step 1: 에디터를 MCP와 함께 띄운다**

```bash
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" D:/GitHub/MintChoco/MintChoco.uproject -skipcompile -ExecCmds="ModelContextProtocol.StartServer"
```

준비 신호: `netstat -an | grep 8000`에 LISTENING.

- [ ] **Step 2: 배치 전 상태를 읽어 둔다**

`UMGToolSet.GetWidgets`로 `WBP_GameHUD`의 트리를 읽어 `CoverageBar`가 아직 없음을 확인하고,
루트 캔버스의 정확한 이름(`CanvasPanel_39`)을 그 결과에서 다시 확인한다. 이름이 다르면 읽은 값을 쓴다.

- [ ] **Step 3: 위젯을 추가한다**

`UMGToolSet.AddWidget`으로 `/Script/MintChoco.PaintCoverageBarWidget`을 `CanvasPanel_39`의 자식으로
`CoverageBar`라는 이름으로 넣는다. 스키마는 호출 직전에 빈 인자로 한 번 불러 확인한다(이 서버는
필수 인자 이름을 오류 메시지로 알려준다).

- [ ] **Step 4: 슬롯 레이아웃을 쓴다**

반환된 슬롯(또는 `GetWidgets`가 알려주는 슬롯)에 `ObjectTools.set_properties`로 한 번에 쓴다:

```json
{
  "LayoutData": {
    "offsets": {"left": 0, "top": 34, "right": 560, "bottom": 40},
    "anchors": {"minimum": {"x": 0.5, "y": 0}, "maximum": {"x": 0.5, "y": 0}},
    "alignment": {"x": 0.5, "y": 0}
  },
  "ZOrder": 1
}
```

`values`는 JSON 문자열로 넘긴다. 쓴 뒤 다시 읽어 확인하는데, 중첩 구조체는 멤버가 camelCase로
돌아오므로 키가 아니라 값으로 비교한다.

- [ ] **Step 5: 컴파일하고 저장한다**

`UMGToolSet.CompileWidgetBlueprint`로 `WBP_GameHUD`를 컴파일해 오류가 없는지 보고, PIE가 꺼진
상태에서 `AssetTools.save_assets`로 `/Game/Assets/UI/Widgets/Game/WBP_GameHUD`를 저장한다.
그 다음 `LogsToolset`으로 `LogBlueprint: Error`가 남지 않았는지 확인한다.

- [ ] **Step 6: 커밋**

```bash
git add Content/Assets/UI/Widgets/Game/WBP_GameHUD.uasset
git commit -m "feat: HUD 상단 중앙에 점유율 게이지 배치"
```

---

### Task 3: PIE 검증

값이 서버에서 와서 클라이언트까지 닿는지는 자동화 테스트가 대신할 수 없다.

**Files:** 없음(확인만)

**Interfaces:**
- Consumes: Task 1, 2
- Produces: 검증 결과

- [ ] **Step 1: 맵을 열고 PIE를 띄운다**

`SceneTools.load_level`로 `/Game/Maps/Lvl_Stage`를 열고, Play As Listen Server, 플레이어 2로 실행한다.

- [ ] **Step 2: 게이지가 보이는지 확인한다**

상단 중앙 타이머 바로 아래에 게이지와 양쪽 포센트가 보이는지. 경기 시작 전에는 둘 다 0%이고
게이지가 비어 있어야 한다.

- [ ] **Step 3: 실시간 갱신을 확인한다**

바닥을 칠하면서 본다. 칠한 쪽 끝에서 게이지가 자라고, 숫자가 0.2초 간격으로 올라가며, 칸이
뚝뚝 끊기지 않고 부드럽게 따라오는지(`InterpSpeed` 1.5).

- [ ] **Step 4: 양쪽이 만나는 모습을 확인한다**

두 플레이어가 서로 다른 색으로 넓게 칠해 합이 100%에 가까워지게 만든다. 두 칸이 겹치지 않고
맞닿으며, 맞닿는 지점이 점유율 많은 쪽으로 밀리는지 본다.

- [ ] **Step 5: 클라이언트 화면도 같은 값인지 확인한다**

호스트 창과 클라이언트 창의 숫자가 (복제 주기만큼의 지연 안에서) 같은지 본다. 다르면 클라이언트가
게임 스테이트 대신 자기 `UPaintSubsystem`을 읽고 있다는 뜻이므로 `ReadCoverage`의 분기를 다시 본다.

- [ ] **Step 6: 로그를 확인한다**

PIE를 멈추고 `Saved/Logs/MintChoco.log`에서 `LogSlate`, `LogBlueprint`, `LogMintChoco` 경고를 훑는다.
에디터가 죽었다면 작업을 멈추고 `Saved/Crashes/*/MintChoco.log`의 `Assertion failed` 줄과 그 앞
몇 줄을 보고한다.

- [ ] **Step 7: 결과를 기록한다**

다섯 확인 항목의 결과를 한 줄씩 남긴다. 크기·색·폰트가 마음에 들지 않는 것은 버그가 아니라
튜닝이며, `CoverageBar`의 디테일 패널에서 `BarHeight`, `TrackColor`, `BorderColor`,
`BorderThickness`, `Font`, `LabelPadding`, `InterpSpeed`, `PollInterval`로 조정한다. 폭과 위치는
캔버스 슬롯에서 바꾼다. 코드는 건드리지 않는다.

---

## 미해결 사항 / 위험

- **`UMGToolSet.AddWidget`의 인자 이름**을 아직 확인하지 않았다. 호출 직전에 빈 인자로 한 번 불러
  스키마를 받아서 맞춘다(이 서버는 오류로 스키마를 돌려준다). 그래도 막히면 위젯 하나를 UI에서
  끌어다 놓는 것은 개발자 요청으로 돌린다: "WBP_GameHUD의 캔버스에 팔레트의
  PaintCoverageBarWidget을 CoverageBar라는 이름으로 넣고, 앵커 상단 중앙, top 34, 폭 560, 높이 40".
- **화면 폭이 좁을 때**: 폭 560은 고정 오프셋이라 1280 이하 해상도에서 화면을 넓게 덮는다. 문제가
  되면 앵커를 (0.2, 0)~(0.8, 0)으로 바꿔 비율로 늘어나게 한다(오프셋 left/right 0).
- **경기 전 점유율**: `WaitingForPlayers` 단계에서도 게이지가 보인다. 숨기고 싶으면 `UGameHudWidget`이
  단계에 따라 `CoverageBar`의 가시성을 조절해야 하는데, 그러려면 `BindWidgetOptional` 프로퍼티를
  추가하는 코드 작업이 붙는다. 지금 스펙에는 없다.
- **절대 점유율의 초반 모습**: 경기 초반에는 양쪽이 몇 %라 게이지가 거의 비어 보인다. 이는 요구사항
  그대로이며, 보기 싫으면 최소 폭 보정이나 상대 비율 모드가 필요하다(디자이너 결정 사항이었고
  절대값으로 확정됐다).
