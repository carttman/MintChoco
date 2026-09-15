# 착탄 스플래시 2차 (2026-09-15 저녁) — 작업 기록

오빠 피드백 네 가지에 대한 대응. 오빠가 자리를 비운 상태라 판단은 내가 하고 여기 적어 둔다.
PIE 육안 확인은 `Lvl_WaterWheel`에 `PaintSplashTestActor`를 놓고 Simulate로 했다(레벨은 저장 안 함).

## 피드백 → 한 일

| 피드백 | 한 일 |
| --- | --- |
| 1. 중앙에서 위로 솟는 제트 방울이 본체 스플랫 안에 작은 원을 남김 | 제트 삭제. 방울은 앞/옆/뒤 세 그룹으로만 던진다. 덤으로, 본체 스플랫 반경 안에 떨어지는 방울은 자국을 안 찍는다(`MarkClearanceScale`, 아래 "덤으로 고친 것"). |
| 2. 토러스 대신 페인트 높이맵을 파문으로 흔들기 | 크라운 토러스 삭제. 착탄점 주변 페인트 높이에 감쇠 링 파동을 더하고(디스플레이스먼트 + 셰이딩 노멀), 페인트가 있는 곳에서만 보인다. |
| 3. 방울 16개, 응집 확대, 뒤로 튀는 방울, 물리 시뮬 느낌 | `MaxDroplets` 16, 접선 성분 비율로 앞 그룹이 커지는 분포(정면 40% → 스침 75%), 나머지의 30%는 뒤로. 큰 것부터 정렬해서 자국은 큰 8개, 점수 팬텀은 큰 4개. 블롭은 16개 모두 웅덩이 테두리에서 끊어지는 가닥으로 그린다. |
| 4. 방울이 바닥 자국보다 밝고 단색 | 팀별 룩 상수(SecondRoughness/Weight, Fuzz)를 `MPC_TeamLook`의 `*Surface2`로 옮기고 `MF_TeamLook` 출력 6~9를 추가. 바닥 레이어(`ML_Look_*`)와 블롭이 같은 출력을 읽는다. 블롭 코트 러프니스 0.25 → 0.12(바닥과 동일), Fuzz 배선, SSS MFP 스케일 0.1. |

## 임의로 정한 것 (오빠가 바꿔도 되는 노브)

- **방울 분포 기본값** (`UPaintSplashProfile`, 카테고리 Launch): `ForwardShareHeadOn 0.4`, `ForwardShareGrazing 0.75`, `BackShareOfRest 0.3`; 그룹별 `Forward{부채 50°, 고도 35~70°(법선 기준), 속도 0.10~0.20×접근속도, 슬라이드 0.15}`, `Side{35°, 25~60°, 0.06~0.14, 0.05}`, `Back{40°, 15~45°, 0.04~0.10, 0}`. 반경은 공 반경의 0.12~0.45배, `DropletRadiusBias 2`(작은 게 많음). `MaxDropletSpeed` 450 → 600.
- **자국/점수 상한**: `MaxMarkDroplets 8`(Niagara는 이 8개만 날린다 — CPU 충돌 트레이스 절감), `MaxScoreDroplets 4`. `DropletHeightAdd` 0.35 → 0.2(자국 8개가 겹쳐 높이 버퍼가 포화되는 걸 줄임). 점수 밸런스는 팬텀 4개 기준이라 예전(4개 전부)과 비슷하지만 방울이 더 커서 반경이 다를 수 있음 → `PhantomCellRadiusScale`로 조정.
- **가닥**: `CohesionRadius 10`, `CohesionDecay 0.35`, `PuddleSpread 0.35`(가닥 뿌리가 바닥을 따라 퍼지는 속도 = 수평 속도의 비율), 가닥은 공 반경 4배보다 길어지면 끊어지고 끊어진 뒤 꼬리는 반경 3배까지만 끌림(HLSL 안 상수). 머티리얼 스칼라 `TailSeconds 0.06`.
- **파문** (`UPaintSplashProfile`, 카테고리 Ripple): `RippleAmplitude 2.5 cm`, `RippleSpeed 80 cm/s`, `RippleWavelength 14 cm`, `RippleDecay 2/s`(수명 ≈ 2 s), `bRipple`. 처음엔 150 cm/s·감쇠 5로 잡았는데 가닥 몸통이 착탄점을 가리는 0.35 s 동안 다 죽어서 안 보였음 → 느리고 오래 가게 바꿈. 파문 여유분 `PaintRippleHeadroom 2 cm`: 마스터 4개 `DisplacementScaling.Magnitude` 3 → 5 (= `PaintMaxHeight` 3 + 2). **불변식이 바뀜**: `Magnitude == PaintMaxHeight + PaintRippleHeadroom` (CLAUDE.md, docs/Traps.md 갱신).
- **룩 일치**: 블롭 `SSSMFPScale` 1 → 0.1. 얇은 가닥은 바닥과 같은 MFP(≈0.45 cm)로도 빛이 통과해 훨씬 밝게 보여서 줄였다. 남는 톤 차이는 형상 차이(바닥 페인트는 텍셀 릴리프로 명암이 생기고 가닥은 매끈)라 재질로는 못 없앤다.
- **그림자**: 블롭 메시 렌더러 `bCastShadows`를 켜 봤다가 다시 껐다. 켜도 화면상 이득이 없었고, 그림자 패스에서 레이마치를 한 번 더 도는 비용만 든다.
- **Fuzz Color**: 바닥 슬랩의 Fuzz Color 출처를 툴로 못 읽어서(다중 출력 노드 표시 문제) 블롭은 `MF_TeamLook.Subsurface`를 넣었다. 바닥이 다른 출력을 쓰면 여기만 맞추면 된다.
- **DA_Splash_Paintball**: `CohesionRadius 10`, `CohesionDecay 0.35`, 파문 셋만 명시값. 나머지는 클래스 기본값(위 표와 같음).

## 덤으로 고친 것

- 방울 자국이 본체 스플랫 위(높이 포화된 판) 안에 떨어지면 붓이 단색 페인트 안에 새 가장자리 거리(B 채널)를 다시 그려서 연한 유령 모양이 남았다(`pie-strands-ripple.png` 이전 캡처에서 확인). `FPaintSplashRequest::SplatRadius`(공 자체 스플랫 반경, `PlayImpactEffect`가 `Deposit.BrushProfile->ComputeRadius`로 채움) × `MarkClearanceScale 1.1` 안에 떨어진 착지는 자국을 건너뛴다.
- `NS_PaintSplash` `Droplets` 이미터 Fixed Bounds ±450: 지난번 세션에서 넣었는데 저장본에 안 남아 있었음(에디터를 강제 종료해서 그런 듯). 다시 넣고 저장 후 `.uasset`에 `::Fixed`가 있는지 확인했다.

## 구현 지도

- C++: `Paint/PaintSplash.h/.cpp`(`EDropletGroup`, `TangentialShare`, `SplitGroups`, `LaunchOffset`, 정렬, `PhantomLandings` 상한, 이름표 `PaintSplashBlob::Drop(i)` / `PaintSplashFX::Drop(i)`), `Paint/PaintSplashProfile.h`(`FPaintSplashDropletGroup`, 노브), `Paint/PaintSplashSubsystem`(Vec4 16슬롯, `MaxMarkDroplets`, 자국 clearance), `Paint/PaintRipple.h/.cpp`(파형·슬롯, 순수 함수), `Paint/PaintableComponent`(`PushRipple`, `ResetRipples`, `PaintRipple{i}`/`PaintRippleShape{i}` MID 파라미터), `Paint/PaintSubsystem.cpp`(`StampSurfaces`에서 본체 스플랫만 파문, `mc.PaintRipple` cvar), `Game/TeamLook`(Surface2), `Weapons/PaintballProfile.cpp`(SplatRadius).
- 테스트: `Splash.Determinism/VolumeCap/PhantomLandings/BlobBounds/HandlerPool` 갱신, 새 `Splash.Groups`, `Ripple.Wave`, `Ripple.Slots`, `TeamLook.Collection`에 Surface2, `Weapons.ProfileAssets`에 개수 검사. 헤드리스 78개 중 실패는 기존 `Audio.Bank`뿐.
- Niagara `NS_PaintSplash`: `User.Drop0..15`(Vector4f) + 16중 삼항(`Particles.UniqueID`)으로 `Particles.Velocity`(`.xyz`)/`Particles.Radius`(`.w`). 옛 `User.Drop{i}Offset/Velocity/Radius`, `User.Crown*` 변수는 C++가 더 안 쓰지만 삭제 툴이 없어 남아 있음(에디터에서 지워도 됨).
- 머티리얼: `M_PaintSplashBlob`(SplashMarch v3: 16 라운드 콘, 웅덩이 뿌리, 광선별 경계구 컬링, 48 스텝, 토러스 없음; `Drop0..15`, `Phys`, `BallRadius`, `PuddleSpread`, `MarchMax`, `TailSeconds`), `MF_TeamLook`(출력 6~9), `ML_Look_Mint/Choco`(로컬 스칼라 대신 출력 6~9), `MF_PaintOverlay`(`PaintRipple0..3`, `PaintRippleShape0..3`, `PaintRippleHeadroom`, Custom `PaintRipple` → Custom `PaintDisplace` → PaintHeight, 파문 기울기를 `MF_PaintNormal.RippleGrad`로), `MF_PaintNormal`(`RippleGrad` 입력), 마스터 4개 Magnitude 5, `MPC_TeamLook`(`MintSurface2`, `ChocoSurface2`).
- 문서: `docs/Traps.md`(Impact splash, 두께 불변식), `docs/UnrealMcp.md`(Vector4 스위즐, MPC 항목 추가, FunctionInput/Output 추가, DisplacementScaling 쓰기, mcprun), `CLAUDE.md`(팀 룩 출력, 두께 문장), `EffectWiring.md`(로컬 전용 파일).

## 검증

- 헤드리스: `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MintChoco; Quit"` 78개 완료, 실패 `MintChoco.Audio.Bank`(기존).
- 셰이더: 머티리얼 재컴파일 뒤 로그의 `LogMaterial: Warning: [AssetLog]`는 배선 전의 "missing input" 과도 경고와 `MF_PaintNormal` 자체 프리뷰 경고("Missing Preview connection for function input 'PaintIdMap'", 함수 썸네일용)뿐.
- Niagara: `GetStackIssues` 경고 0, `GetSystemCompileState` 에러 없음.
- PIE(Simulate) 캡처 `pie-strands-ripple.png`: 왼쪽부터 가닥이 웅덩이에서 솟는 순간, 끊어져 나가는 물방울, 파문 링, 자국. `sdf-preview-*.png`는 머티리얼 적용 전 파이썬으로 같은 SDF를 그려 본 실루엣(비스듬/정면).

## 남은 것 / 튜닝 후보

- 가닥 톤: 바닥보다 여전히 조금 밝고 매끈함. 원하면 `M_PaintSplashBlob`의 `SSSMFPScale`(0.1)과 코트 가중치(WetCoat)를 더 낮추거나, 가닥 노멀에 약한 노이즈를 더해 릴리프 느낌을 흉내낼 수 있음.
- 파문은 가닥 몸통 때문에 처음 0.3 s가 가려짐. 더 일찍 보이게 하려면 `CohesionDecay`를 줄이거나 `RippleSpeed`를 올리면 됨.
- 방울이 250 cm(`MaxTravel`) 밖으로 나가면 큐브 밖이라 잘림. 스치는 히트에서 앞 무리가 빠르니 `MaxTravel`을 300으로 올리는 것도 방법(큐브가 커져 화면 점유가 늘어나지만 광선 컬링이 있어 비용은 괜찮음).
- 이미 칠해진 페인트 위에 떨어지는 방울 자국은 연한 윤곽으로 보인다(붓이 단색 페인트 안에 새 가장자리 거리를 그리는 기존 파이프라인 동작). 본체 스플랫 안은 clearance로 막았지만 그 바깥의 옛 페인트 위는 그대로다. 페인트 안에 찍히는 스탬프의 B 채널을 `max`로 합치면 해결될 텐데 붓 머티리얼(`M_PaintBrush`) 쪽 일이라 이번엔 안 건드렸다.
- 스크래치 도구는 세션 스크래치패드 `b5c7b697-...\scratchpad\tools\`에 있음: `sdf_preview16.py`(순수 파이썬 레이마치 프리뷰), `blob16_m_paintsplashblob.py`, `ns16_droplets.py`, `d_teamlook.py`, `c_ripple_materials.py`, `mcprun.ps1`(긴 스크립트를 MCP HTTP로 실행), `capture.ps1`, `sheet.py`.
