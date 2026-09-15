#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"

#include "WarmupSettings.generated.h"

class APaintProjectile;

/** 미리 만들어 둘 투사체 한 종류. */
USTRUCT()
struct FWarmupProjectileEntry
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	TSoftClassPtr<APaintProjectile> ProjectileClass;

	/** 풀에 미리 재워 둘 개수. 교전 중 동시 최대치 로그를 보고 맞춘다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup", meta = (ClampMin = "0", ClampMax = "512"))
	int32 Count = 64;
};

/**
 * 경기 시작 전에 미리 데워 둘 것들. Config/DefaultGame.ini에 남는다.
 *
 * 워밍업은 로딩 화면 뒤에서 돌고, 끝날 때까지 가림막이 열리지 않는다. 그래서 여기 값을
 * 늘리면 로딩이 길어지는 대신 경기 중 히칭이 줄어든다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Warmup"))
class MINTCHOCO_API UWarmupSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UWarmupSettings& Get() { return *GetDefault<UWarmupSettings>(); }

	/** 전체 스위치. 끄면 워밍업이 아무것도 하지 않고 가림막도 잡지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bEnabled = true;

	/**
	 * 게임 맵(AGameGameState가 있는 월드)에서만 돈다. 타이틀과 로비까지 붙잡으면 첫 화면이
	 * 늦어지는데, 정작 데울 것은 경기 자산이라 얻는 것이 없다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup")
	bool bGameMapsOnly = true;

	/** 아이템 프로필과 픽업 클래스를 미리 로드한다. 첫 아이템 획득의 동기 로드를 없앤다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup|Steps")
	bool bPreloadItems = true;

	/** PSO 프리캐싱이 끝날 때까지 기다린다. r.PSOPrecaching이 꺼져 있으면 즉시 통과한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup|Steps")
	bool bWaitForPSOPrecache = true;

	/** 페인트볼을 미리 만들어 풀에 넣는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup|Steps")
	bool bPrewarmProjectilePool = true;

	/** 어떤 투사체를 몇 개씩. 비워 두면 풀 예열은 건너뛴다. */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup|Steps", meta = (TitleProperty = "ProjectileClass"))
	TArray<FWarmupProjectileEntry> ProjectilePool;

	/**
	 * 워밍업 전체의 상한(초). 넘으면 경고를 남기고 가림막을 놓아 준다. 어떤 단계가 영영
	 * 끝나지 않아도 로딩 화면에 갇히지 않게 하는 안전장치다.
	 *
	 * 이 값보다 ScreenFadeSettings의 MaxReadyWait이 작으면 그쪽이 먼저 화면을 열어 버린다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Warmup", meta = (ClampMin = "0", ForceUnits = "s"))
	float Timeout = 15.0f;
};
