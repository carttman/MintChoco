# 점유율 게이지 HUD 스펙

2026-09-11, 게임 디자이너 요청.

## 목표

경기 중 팀별 점유율을 화면 상단 중앙에서 실시간으로 읽을 수 있게 한다. 게이지는 양 끝에서
가운데로 차오르고, 둘이 만나는 지점은 점유율이 많은 쪽으로 밀린다.

## 요구사항

- 위치: 상단 중앙. 이미 그 자리에 있는 타이머(`Txt_Timer`) **아래**에 둔다.
- 모양: 왼쪽 끝에서 민트가, 오른쪽 끝에서 초코가 차오른다. 가운데 남은 틈이 미채색 면적이다.
- 값: **절대 점유율**. 민트 20% / 초코 15%면 가운데 65%가 빈 틈으로 남는다. 둘이 차오르다
  맞닿는 지점은 점유율이 많은 쪽으로 밀린다.
- 숫자: 게이지 위 양쪽 끝에 팀별 포센트를 함께 보여준다.
- 실시간: 서버가 측정한 값이 복제되는 주기(0.2초)마다 갱신되고, 그 사이는 부드럽게 따라간다.

## 결정 사항 (디자이너 확답)

1. **채울 기준**: 절대 점유율. 상대 비율(항상 꽉 찬 게이지)이 아니다.
2. **배치**: 타이머는 그대로 두고 게이지를 그 아래에 둔다.
3. **숫자**: 양쪽 포센트 표시.

## 조사로 확정된 사실

- `UPaintCoverageBarWidget`(`Source/MintChoco/Paint/PaintCoverageBarWidget.h`)이 **이미 요구사항
  그대로 구현되어 있다**: `NativePaint`에서 민트를 왼쪽, 초코를 오른쪽에서 그리고, 초코를
  `1 - Mint`로 클램프해 두 칸이 겹치지 않게 한다. 포센트 라벨도 그린다. 폰트 기본값도
  생성자에서 채워져 있다(`FCoreStyle::GetDefaultFontStyle("Bold", 14)`).
- 그런데 이 클래스를 **C++도 콘텐츠도 아무도 참조하지 않는다**. 그래서 화면에 없다.
- 값의 출처: `AGameGameState::WorldCoverage`(`FPaintCoverage`, 서버 셀 그리드 합, `RefreshCoverage`가
  `CoverageRefreshInterval` 0.2초마다 갱신 후 복제). 위젯의 `ReadCoverage`가 게임 스테이트를 먼저
  보고, 없으면(샘플 맵) `UPaintSubsystem::GetWorldCoverage()`로 떨어진다. 팀 id는
  `Teams::Mint = 0`, `Teams::Choco = 1`.
- `WBP_GameHUD`(부모 `UGameHudWidget`)는 `CanvasPanel_39` 루트에 `ChargeRing`(`UPaintChargeWidget`,
  **C++ 위젯을 위젯 트리에 직접 배치한 선례**), `Txt_Timer`, `Img_Item`, `Txt_Item`, `Txt_Countdown`을
  갖는다. 따라서 게이지도 위젯 블루프린트를 새로 만들지 않고 C++ 클래스를 바로 배치하면 된다.
- `Txt_Timer` 슬롯: 앵커 (0.5, 0), 정렬 (0.5, 0), offsets left 0 / top 0 / right 100 / bottom 30.
  즉 상단 중앙 0~30px를 차지한다. 게이지는 그 아래 34px부터 둔다.
- `Content/Maps/Lvl_Stage.umap`은 GameMode를 `BP_GameMode`로 오버라이드하므로 `AGameGameState`가
  있고 복제 경로가 살아 있다.

## 범위 밖

- 미니맵, 점유율 추이 그래프.
- 팀 3개 이상.
- 매치 종료 화면의 결과 표시(`WBP_GameResultPopup`은 이미 별도로 있다).

## 주의

- `Lvl_Stage`라는 이름의 맵이 두 개 있다: `/Game/Maps/Lvl_Stage`(BP_GameMode를 쓰는 실제 무대)와
  `/Game/Sample/Maps/Lvl_Stage`(샘플). 이 작업의 검증은 앞쪽에서 한다.
