#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"

#include "LookSubsystem.generated.h"

class APostProcessVolume;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class ULookPreset;
class ULookReviewSetup;
class UMaterialInterface;
class UMeshComponent;
class USceneComponent;
class USkyLightComponent;
class UStaticMeshComponent;
struct FLookFogSettings;
struct FLookMaterialSwap;
struct FLookSkyDomeSettings;
struct FLookSkyLightSettings;
struct FLookSunSettings;

/**
 * 콘솔 변수를 바꾸기 전 원래 문자열을 기억했다가 되돌린다. 콘솔 변수는 프로세스 전역이라
 * PIE 월드가 사라져도 남으므로 바꾼 쪽이 반드시 되돌려야 한다.
 */
struct MINTCHOCO_API FLookConsoleVariableBackup
{
	/** 없는 변수면 false. 같은 이름을 여러 번 바꿔도 처음 값만 기억한다. */
	bool Set(const FString& Name, const FString& Value);

	/** 기억한 값을 모두 되돌리고 비운다. */
	void RestoreAll();

	bool IsEmpty() const { return Originals.IsEmpty(); }

private:
	TMap<FString, FString> Originals;
};

namespace LookPreset
{
	/**
	 * mc.Look 인자를 목록 인덱스로 바꾼다. "Off" 와 "0" 은 INDEX_NONE(끄기),
	 * "1".."N" 은 목록 순서, 나머지는 ShortName(대소문자 무시). 알아들을 수 없으면 false.
	 */
	MINTCHOCO_API bool ResolveArgument(const FString& Argument, const TArray<FName>& ShortNames, int32& OutIndex);
}

/** 해의 원래 값. */
struct FLookSunState
{
	TWeakObjectPtr<UDirectionalLightComponent> Light;
	float Intensity = 0.0f;
	float Temperature = 6500.0f;
	float SourceAngle = 0.0f;
	float ShadowAmount = 1.0f;
	float SpecularScale = 1.0f;
};

/** 스카이라이트의 원래 값. */
struct FLookSkyLightState
{
	TWeakObjectPtr<USkyLightComponent> Light;
	float Intensity = 0.0f;
	FLinearColor LightColor = FLinearColor::White;
	FLinearColor LowerHemisphereColor = FLinearColor::Black;
};

/** 높이 안개의 원래 값. */
struct FLookFogState
{
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	float Density = 0.0f;
	FLinearColor InscatteringColor = FLinearColor::Black;
	float StartDistance = 0.0f;
};

/** 머티리얼을 바꿔 끼운 슬롯 하나. 되돌릴 때 아직 바꾼 머티리얼이 끼워져 있을 때만 원래 것으로 돌린다. */
USTRUCT()
struct FLookSwappedSlot
{
	GENERATED_BODY()

	TWeakObjectPtr<UMeshComponent> Mesh;

	int32 Slot = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Original;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Replacement;
};

/**
 * 룩 프리셋을 월드에 런타임으로만 얹는다. 레벨 액터와 에셋은 디스크에서 바뀌지 않는다.
 *
 * 얹기 전에 해, 스카이라이트, 안개, 구름, 스카이돔, 콘솔 변수의 원래 값을 기억하고,
 * 프리셋의 후처리는 경계 없는 트랜지언트 볼륨으로 레벨 볼륨 위에 올린다.
 * Restore 는 그 반대이고 월드가 사라질 때도 불린다.
 *
 * ULookSettings 의 리뷰 설정이 채워져 있으면 월드 시작 때 그 프리셋을 얹고, 리뷰 셋업의
 * 마네킹을 세우고, 표면이 준비될 틈을 둔 뒤 페인트를 찍는다. 비교 캡처용이다.
 */
UCLASS()
class MINTCHOCO_API ULookSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** 지금 얹힌 프리셋을 걷어내고 새로 얹는다. nullptr 이면 걷어내기만 한다. */
	UFUNCTION(BlueprintCallable, Category = "Look")
	void ApplyPreset(ULookPreset* Preset);

	/** 레벨 원래 룩으로 돌아간다. */
	UFUNCTION(BlueprintCallable, Category = "Look")
	void Restore();

	UFUNCTION(BlueprintPure, Category = "Look")
	ULookPreset* GetActivePreset() const { return ActivePreset; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** bWorldEnding 이면 전역 상태(콘솔 변수)만 되돌리고 액터는 건드리지 않는다. */
	void RestoreState(bool bWorldEnding);

	void SpawnVolume(UWorld& World, const ULookPreset& Preset);
	void ApplySun(UWorld& World, const FLookSunSettings& Settings);
	void ApplySkyLight(UWorld& World, const FLookSkyLightSettings& Settings);
	void ApplyFog(UWorld& World, const FLookFogSettings& Settings);
	void HideClouds(UWorld& World);
	void ApplySkyDome(UWorld& World, const FLookSkyDomeSettings& Settings);
	void ApplyMaterialSwaps(UWorld& World, const TArray<FLookMaterialSwap>& Swaps);

	/** 월드의 메시 슬롯을 훑어 스왑 표에 있는 머티리얼을 바꿔 끼운다. 리스폰과 카메라 페이드 뒤를 따라잡는다. */
	void RescanSwaps();

	void StartReview(ULookReviewSetup& Setup);
	void StampReviewSplats();

	UPROPERTY(Transient)
	TObjectPtr<ULookPreset> ActivePreset;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Volume;

	UPROPERTY(Transient)
	TObjectPtr<ULookReviewSetup> ReviewSetup;

	/** 바꿀 머티리얼 → 바꿔 끼울 머티리얼. 프리셋이 켜진 동안 둘 다 살아 있게 붙잡는다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInterface>> SwapTable;

	UPROPERTY(Transient)
	TArray<FLookSwappedSlot> SwappedSlots;

	FLookSunState SunState;
	FLookSkyLightState SkyLightState;
	FLookFogState FogState;

	TArray<TWeakObjectPtr<USceneComponent>> HiddenClouds;

	/** MID 로 바꾼 스카이돔 슬롯과 원래 머티리얼. */
	TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, int32>> DomeSlots;
	TWeakObjectPtr<UMaterialInterface> DomeMaterial;

	FLookConsoleVariableBackup ConsoleVariables;

	FTimerHandle SwapRescanTimer;
	FTimerHandle ReviewSplatTimer;
};
