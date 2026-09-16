#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "LookSettings.generated.h"

class ULookPreset;
class ULookReviewSetup;
class UMaterialInterface;

/** 룩 프리셋 목록과 비교 캡처용 리뷰 설정. Config/DefaultGame.ini 에 남는다. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Look"))
class MINTCHOCO_API ULookSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULookSettings();

	static const ULookSettings& Get() { return *GetDefault<ULookSettings>(); }

	/** mc.Look 번호 순서. 1 이 첫 항목이고 0 은 끄기다. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TArray<TSoftObjectPtr<ULookPreset>> Presets;

	/** 스카이돔 메시가 쓰는 머티리얼. 이 머티리얼을 쓰는 슬롯에만 프리셋의 SkyDome 값이 들어간다. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UMaterialInterface> SkyDomeMaterial;

	/** 프리셋 볼륨의 우선순위. 레벨에 놓인 볼륨보다 커야 이긴다. */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	float PostProcessPriority = 1000.0f;

	/** 에디터 세션 동안만 쓴다. 비교 캡처가 PIE/Simulate 를 시작하기 전에 채우고, 저장되지 않는다. */
	UPROPERTY(Transient, EditAnywhere, Category = "Review")
	TSoftObjectPtr<ULookPreset> ReviewPreset;

	UPROPERTY(Transient, EditAnywhere, Category = "Review")
	TSoftObjectPtr<ULookReviewSetup> ReviewSetup;
};
