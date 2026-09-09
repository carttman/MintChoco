#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ScreenFadeSettings.generated.h"

class UUserWidget;

/** 맵 전환 가림막 설정. Config/DefaultGame.ini에 남는다. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Screen Fade"))
class MINTCHOCO_API UScreenFadeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UScreenFadeSettings& Get() { return *GetDefault<UScreenFadeSettings>(); }

	/**
	 * 가림막 위젯. 검은 배경 위에 로딩 문구나 스피너를 얹은 UMG 위젯이다. 비어 있으면
	 * 단색 검은 화면으로 대신한다. 입력을 가로채지 않도록 HitTestInvisible로 얹힌다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Screen Fade")
	TSoftClassPtr<UUserWidget> LoadingWidgetClass;

	/** 페이드 아웃과 페이드 인 각각에 걸리는 시간(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Screen Fade", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FadeDuration = 0.5f;

	/** 완전히 어두워진 뒤 트래블을 시작하기까지 더 기다리는 시간(초). 늦은 클라이언트의 페이드가 끝날 여유다. */
	UPROPERTY(Config, EditAnywhere, Category = "Screen Fade", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float TravelHold = 0.2f;

	/**
	 * 새 맵이 준비되기를 기다리는 최대 시간(초). 이 시간이 지나면 준비 조건과 무관하게
	 * 화면을 연다. 어떤 홀드가 영원히 풀리지 않아도 검은 화면에 갇히지 않게 하는 안전장치다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Screen Fade", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxReadyWait = 10.0f;
};
