# Hybrid 룩 굽기 — 굽기 전 값 기록

작성 2026-09-16, 브랜치 `stylized`, 굽기 직전 HEAD `16efa13`.

`DA_Look_Hybrid`를 `Lvl_Stage`·`Lvl_Stage_inside`와 공용 에셋에 영구 반영하면서 덮어쓴
값들이다. **되돌릴 때 이 문서와 `DA_Look_Baseline`을 쓴다.** 되돌리는 가장 빠른 길:

```
git checkout -- Content/Maps/Lvl_Stage.umap Content/Maps/Lvl_Stage_inside.umap \
                Content/Assets/Environment/Sky/MI_StageSkyDome.uasset \
                Content/Assets/Paint/Materials/Team/MPC_TeamLook.uasset
```

런타임 A/B 는 `mc.Look Baseline`. 다만 Baseline 프리셋이 복원하지 **못하는** 것이 둘 있다:
볼류메트릭 구름(`bHideVolumetricCloud`는 단방향이라 `VolumetricCloud_0`의 Visible 체크박스를
직접 켜야 한다)과 팀 광택(팀 색은 이제 `MPC_TeamLook`만 결정한다).

---

## 두 맵의 굽기 전 값

`Lvl_Stage`와 `Lvl_Stage_inside`의 해당 액터 값은 굽기 전에 **서로 완전히 같았다**.

### PostProcessVolume_0

| 항목 | 값 |
|---|---|
| `bUnbound` / `bEnabled` / `BlendWeight` / `Priority` | true / true / 1.0 / 0.0 |
| 켜져 있던 오버라이드 | `bOverride_AutoExposureBias` **하나뿐** |
| `AutoExposureBias` | 1.5 |
| `WeightedBlendables` | 비어 있음 |

### DirectionalLight_0.LightComponent0

| 속성 | 값 |
|---|---|
| `Intensity` | 12.5 |
| `Temperature` | 6500 (`bUseTemperature` = true) |
| `LightSourceAngle` | 0.7357 |
| `ShadowAmount` | 1.0 |
| `SpecularScale` | 1.0 |
| `Mobility` | Movable |
| `bAtmosphereSunLight` | true |

### 나머지 액터

| 오브젝트 | 값 |
|---|---|
| `SkyLight_0.SkyLightComponent0` | Intensity 4.5, LightColor (1,1,1,1), LowerHemisphereColor (0,0,0,1), Movable |
| `ExponentialHeightFog_0.HeightFogComponent0` | FogDensity 0.02, FogInscatteringLuminance (0,0,0,1), StartDistance 0 |
| `VolumetricCloud_0.VolumetricCloudComponent` | `bVisible` = **true** |

스카이라이트와 안개는 Hybrid가 오버라이드하지 않아 **건드리지 않았다**. 기록용이다.

## 공용 에셋의 굽기 전 값

### MI_StageSkyDome

스칼라만 오버라이드하고 있었다: `Brightness` 5.0, `GradientPower` 0.65.
`VectorParameterValues`는 비어 있었고, 따라서 실제로 쓰이던 색은 마스터
`M_StageSkyDome`의 파라미터 기본값이다:

| 파라미터 | 굽기 전 (마스터 기본값) |
|---|---|
| `TopColor` | (0.03433981, 0.33245152, 0.80695224, 1) |
| `BottomColor` | (0.46207699, 0.84687322, 0.73791039, 1) |

### MPC_TeamLook

| 파라미터 | 굽기 전 | GUID |
|---|---|---|
| `MintColor` | (0.35, 0.9, 0.7, 1) | A3D9A15B-4277-B2F4-2D07-10B61FE5806A |
| `MintSubsurface` | (0.175, 0.45, 0.35, 1) | 40DECA72-4E1A-E115-DA60-F78F65D487A6 |
| `MintSurface` | (0.5, 0.45, 0, 0.7) | 981DBF69-4ABD-1051-92E9-D7AF8ED83A35 |
| `ChocoColor` | (0.32, 0.18, 0.1, 1) | 156E908D-45E9-D619-F1A4-46B0BE2A3A09 |
| `ChocoSubsurface` | (0.11, 0.06, 0.034, 1) | 29055523-4E4B-7F07-8773-DB9ED4A9AC44 |
| `ChocoSurface` | (0.22, 0.7, 0, 0) | D81C7A8B-4E60-2609-FE3D-FDB40B289B99 |

`Surface`는 `(Roughness, Specular, Metallic, WetCoat)` 순서다.

## 굽기 전 실효 후처리 값

볼륨이 오버라이드하던 필드는 `AutoExposureBias` 하나뿐이었으므로, 나머지 17개의 굽기 전
실효값은 **구조체 기본값**이되 프로젝트의 `r.DefaultFeature.*` 가 바꾼 것만 다르다.
이 표가 `DA_Look_Baseline`의 내용이다.

| 필드 | 굽기 전 실효값 | 출처 |
|---|---|---|
| `ColorSaturation` | (1, 1, 1, 1) | 구조체 기본값 |
| `ColorGainShadows` | (1, 1, 1, 1) | 구조체 기본값 |
| `ToneCurveAmount` | 1.0 | 구조체 기본값 |
| `SceneFringeIntensity` | 0.0 | 구조체 기본값 |
| `BloomIntensity` | 0.675 | 구조체 기본값 (`r.DefaultFeature.Bloom` = 1) |
| `AutoExposureMethod` | AEM_Histogram | `r.DefaultFeature.AutoExposure.Method` = 0 |
| `AutoExposureMinBrightness` | -10 | EV100 (`ExtendDefaultLuminanceRange` = 1) |
| `AutoExposureMaxBrightness` | 20 | 같음 |
| `AutoExposureBias` | **1.5** | 레벨 볼륨의 유일한 오버라이드 |
| `LocalExposureHighlightContrastScale` | **0.8** | `r.DefaultFeature.LocalExposure.HighlightContrastScale` |
| `LocalExposureShadowContrastScale` | **0.8** | 같은 계열 cvar |
| `LensFlareIntensity` | **0.0** | `r.DefaultFeature.LensFlare` = 0 (구조체 기본값은 1.0) |
| `VignetteIntensity` | 0.4 | 구조체 기본값 |
| `FilmGrainIntensity` | 0.0 | 구조체 기본값 |
| `MotionBlurAmount` | 0.5 | 구조체 기본값 (`r.DefaultFeature.MotionBlur` = 1) |
| `LumenDiffuseColorBoost` | 1.0 | 구조체 기본값 |
| `LumenSkylightLeaking` | 0.0 | 구조체 기본값 |
| `LumenMaxRoughnessToTraceReflections` | 0.4 | 구조체 기본값 |

굵게 표시한 셋이 함정이다. 구조체 기본값만 보고 Baseline을 만들면 원래 룩이 복원되지 않는다.

---

## 실제로 쓴 값 (Hybrid)

`DA_Look_Hybrid`에서 읽어 그대로 옮겼다. `bOverride*`가 켜진 필드만 썼다.

### 두 맵의 PostProcessVolume_0 — 18개 오버라이드

```
ColorSaturation (1.2, 1.2, 1.2, 1)      ColorGainShadows (0.96, 0.96, 1.08, 1)
ToneCurveAmount 0.4                     SceneFringeIntensity 0
BloomIntensity 0.4                      AutoExposureMethod AEM_Histogram
AutoExposureMinBrightness 2             AutoExposureMaxBrightness 2
AutoExposureBias 0                      LocalExposureHighlightContrastScale 1
LocalExposureShadowContrastScale 1      LensFlareIntensity 0
VignetteIntensity 0                     FilmGrainIntensity 0
MotionBlurAmount 0                      LumenDiffuseColorBoost 1.3
LumenSkylightLeaking 0.2                LumenMaxRoughnessToTraceReflections 0.2
```

`WeightedBlendables`에 `/Game/Assets/Look/PP/MI_PP_LookStylize_Hybrid` 를 Weight 1.0 으로
한 개 추가. `bUnbound`/`bEnabled`/`BlendWeight`/`Priority` 는 이미 맞아서 건드리지 않았다.
특히 `Priority`는 0 그대로다 — 런타임 오버레이가 쓰던 1000은 레벨 볼륨을 이기려던 값이라
구운 뒤에는 의미가 없다.

### 그 밖

| 대상 | 쓴 값 |
|---|---|
| `DirectionalLight_0.LightComponent0` | Temperature 5800, LightSourceAngle 0.3, SpecularScale 0.7 (Intensity·ShadowAmount는 그대로) |
| `VolumetricCloud_0.VolumetricCloudComponent` | `bVisible` = false |
| `MI_StageSkyDome` | TopColor (0.12, 0.38, 1, 1), BottomColor (0.78, 0.92, 1, 1) |
| `MPC_TeamLook` | `MintSurface` R 0.5 → **0.6**, `ChocoSurface` R 0.22 → **0.32** (러프니스 +0.1). 나머지 4개와 모든 GUID는 그대로 |

## 프리셋에서 걷어낸 팀 값

팀 색을 `MPC_TeamLook` 하나로 모으면서 `ULookPreset::TeamLookValues`를 지웠다.
지우기 전 각 프리셋이 들고 있던 값이다. 나중에 필요하면 MPC에서 직접 만든다.

| 프리셋 | MintSurface | ChocoSurface |
|---|---|---|
| Hybrid | (0.6, 0.45, 0, 0.7) | (0.32, 0.7, 0, 0) — 이제 MPC에 구워짐 |
| Toon | (0.6, 0.3, 0, 0) | (0.6, 0.3, 0, 0) |
| SoftPBR | 없음 | 없음 |

`TeamLook.cpp`의 폴백 표 `TeamLookDefaults[]`도 같이 지웠다. 지우기 전 값:

```
Mint  Color (0.35, 0.9, 0.7)   Subsurface (0.175, 0.45, 0.35)   R 0.50  S 0.45  M 0  W 0.70
Choco Color (0.32, 0.18, 0.1)  Subsurface (0.11, 0.06, 0.034)   R 0.22  S 0.70  M 0  W 0.0
```

---

## 확인한 것

- `FPostProcessSettings` 부분 쓰기는 **병합**이다. 스크래치(`DA_Look_Baseline`)에서 두 필드를
  쓴 뒤 한 필드만 다시 써 보니 나머지가 살아 있었다. 레벨 볼륨의 기존
  `bOverride_AutoExposureBias`도 그대로 남았다.
- `WeightedBlendables`는 `Settings` 스칼라 쓰기와 **별도 호출**로 넣었고, 넣기 직전 배열을
  다시 읽어 비어 있음을 확인했다.
- 해는 두 맵 모두 `Movable`이라 런타임 setter가 무시되는 Static 모빌리티 문제는 없다.
  `bUseTemperature`도 원래 켜져 있어서 5800K가 실제로 먹는다.
- 굽고 나서 에디터 뷰포트(PIE 아님)에서 셀 음영과 외곽선이 보인다. 이번 작업의 합격선이었다.
