#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Scene.h"

#include "LookPreset.generated.h"

class UMaterialInterface;

/** 레벨의 해(대기 태양으로 쓰는 방향광)에 덮어쓸 값. 켠 항목만 바뀐다. */
USTRUCT(BlueprintType)
struct FLookSunSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (InlineEditConditionToggle))
	bool bOverrideIntensity = false;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (EditCondition = "bOverrideIntensity", ClampMin = "0", ForceUnits = "lux"))
	float Intensity = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (InlineEditConditionToggle))
	bool bOverrideTemperature = false;

	/** 해의 Use Temperature 가 켜져 있을 때만 먹는다. */
	UPROPERTY(EditAnywhere, Category = "Sun", meta = (EditCondition = "bOverrideTemperature", ClampMin = "1700", ClampMax = "12000"))
	float Temperature = 6500.0f;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (InlineEditConditionToggle))
	bool bOverrideSourceAngle = false;

	/** 도 단위. 작을수록 그림자 경계가 날카롭다. */
	UPROPERTY(EditAnywhere, Category = "Sun", meta = (EditCondition = "bOverrideSourceAngle", ClampMin = "0", ClampMax = "5"))
	float SourceAngle = 0.5357f;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (InlineEditConditionToggle))
	bool bOverrideShadowAmount = false;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (EditCondition = "bOverrideShadowAmount", ClampMin = "0", ClampMax = "1"))
	float ShadowAmount = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (InlineEditConditionToggle))
	bool bOverrideSpecularScale = false;

	UPROPERTY(EditAnywhere, Category = "Sun", meta = (EditCondition = "bOverrideSpecularScale", ClampMin = "0"))
	float SpecularScale = 1.0f;
};

/** 레벨 스카이라이트에 덮어쓸 값. 켠 항목만 바뀐다. */
USTRUCT(BlueprintType)
struct FLookSkyLightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (InlineEditConditionToggle))
	bool bOverrideIntensity = false;

	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (EditCondition = "bOverrideIntensity", ClampMin = "0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (InlineEditConditionToggle))
	bool bOverrideLightColor = false;

	/** 그림자 쪽 색을 정하는 값이다. 보라색 그림자는 알베도가 아니라 여기서 만든다. */
	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (EditCondition = "bOverrideLightColor", HideAlphaChannel))
	FLinearColor LightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (InlineEditConditionToggle))
	bool bOverrideLowerHemisphereColor = false;

	UPROPERTY(EditAnywhere, Category = "Sky Light", meta = (EditCondition = "bOverrideLowerHemisphereColor", HideAlphaChannel))
	FLinearColor LowerHemisphereColor = FLinearColor::Black;
};

/** 레벨 높이 안개에 덮어쓸 값. 켠 항목만 바뀐다. */
USTRUCT(BlueprintType)
struct FLookFogSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (EditCondition = "bOverrideDensity", ClampMin = "0"))
	float Density = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (InlineEditConditionToggle))
	bool bOverrideInscatteringColor = false;

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (EditCondition = "bOverrideInscatteringColor", HideAlphaChannel))
	FLinearColor InscatteringColor = FLinearColor(0.45f, 0.55f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (InlineEditConditionToggle))
	bool bOverrideStartDistance = false;

	UPROPERTY(EditAnywhere, Category = "Fog", meta = (EditCondition = "bOverrideStartDistance", ClampMin = "0", ForceUnits = "cm"))
	float StartDistance = 0.0f;
};

/** ULookSettings::SkyDomeMaterial 을 쓰는 스카이돔에 넣을 파라미터. 켠 항목만 바뀐다. */
USTRUCT(BlueprintType)
struct FLookSkyDomeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (InlineEditConditionToggle))
	bool bOverrideTopColor = false;

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (EditCondition = "bOverrideTopColor"))
	FLinearColor TopColor = FLinearColor(0.2f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (InlineEditConditionToggle))
	bool bOverrideBottomColor = false;

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (EditCondition = "bOverrideBottomColor"))
	FLinearColor BottomColor = FLinearColor(0.8f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (InlineEditConditionToggle))
	bool bOverrideBrightness = false;

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (EditCondition = "bOverrideBrightness", ClampMin = "0"))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (InlineEditConditionToggle))
	bool bOverrideGradientPower = false;

	UPROPERTY(EditAnywhere, Category = "Sky Dome", meta = (EditCondition = "bOverrideGradientPower", ClampMin = "0"))
	float GradientPower = 1.0f;
};

/** 프리셋이 켜져 있는 동안만 바꿔 두는 콘솔 변수. */
USTRUCT(BlueprintType)
struct FLookConsoleVariable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Console Variable")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Console Variable")
	FString Value;
};

/** 월드의 메시 슬롯 중 From 을 쓰는 곳을 To 로 바꿔 끼운다. */
USTRUCT(BlueprintType)
struct FLookMaterialSwap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material Swap")
	TSoftObjectPtr<UMaterialInterface> From;

	UPROPERTY(EditAnywhere, Category = "Material Swap")
	TSoftObjectPtr<UMaterialInterface> To;
};

/**
 * 렌더링 룩 한 벌. ULookSubsystem 이 런타임에만 얹으므로 레벨과 원본 에셋은 바뀌지 않는다.
 * 목록은 ULookSettings::Presets, 전환은 mc.Look.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API ULookPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** mc.Look 이 받는 이름. 대소문자는 가리지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Look")
	FName ShortName;

	UPROPERTY(EditAnywhere, Category = "Look", meta = (MultiLine = true))
	FText Description;

	/** 레벨 볼륨보다 우선하는 경계 없는 볼륨으로 얹힌다. bOverride 가 켜진 항목만 덮는다. */
	UPROPERTY(EditAnywhere, Category = "Post Process")
	FPostProcessSettings PostProcess;

	UPROPERTY(EditAnywhere, Category = "Lighting")
	FLookSunSettings Sun;

	UPROPERTY(EditAnywhere, Category = "Lighting")
	FLookSkyLightSettings SkyLight;

	UPROPERTY(EditAnywhere, Category = "Atmosphere")
	FLookFogSettings Fog;

	UPROPERTY(EditAnywhere, Category = "Atmosphere")
	FLookSkyDomeSettings SkyDome;

	/** 볼류메트릭 구름을 감춘다. 실사 느낌이 가장 강하게 남는 요소다. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere")
	bool bHideVolumetricCloud = false;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	TArray<FLookConsoleVariable> ConsoleVariables;

	UPROPERTY(EditAnywhere, Category = "Materials")
	TArray<FLookMaterialSwap> MaterialSwaps;
};
