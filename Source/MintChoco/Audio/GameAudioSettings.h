#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "Game/GameGameState.h"

#include "GameAudioSettings.generated.h"

class USoundAttenuation;
class USoundBank;
class USoundClass;
class USoundMix;

/**
 * 게임 전체가 공유하는 오디오 설정. Config/DefaultGame.ini에 남는다.
 *
 * 어떤 소리가 나는지는 뱅크가, 얼마나 크게 나는지는 사운드 클래스가 정하고, 여기에는
 * "그 둘이 어디 있는지"와 소리마다 달라질 이유가 없는 값만 둔다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Game Audio"))
class MINTCHOCO_API UGameAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGameAudioSettings& Get() { return *GetDefault<UGameAudioSettings>(); }

	/** 이벤트 태그를 소리로 바꾸는 표. 비어 있으면 게임은 조용히 돌아간다. */
	UPROPERTY(Config, EditAnywhere, Category = "Bank")
	TSoftObjectPtr<USoundBank> Bank;

	/** 이벤트가 감쇠를 따로 지정하지 않았을 때 쓰는 기본값. */
	UPROPERTY(Config, EditAnywhere, Category = "Bank")
	TSoftObjectPtr<USoundAttenuation> DefaultAttenuation;

	//~ 음악

	/**
	 * 매치 단계마다 틀 곡. 서버가 단계를 바꾸면 모든 머신이 복제된 값을 보고 각자 갈아탄다.
	 * 등록되지 않은 단계는 곡을 바꾸지 않고 하던 것을 이어간다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	TMap<EMatchPhase, FGameplayTag> MusicByPhase;

	/** 로비·타이틀처럼 매치 단계가 없는 맵에서 틀 곡. */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	FGameplayTag LobbyMusic;

	/** 곡을 갈아탈 때 겹치는 시간(초). 0이면 딱 끊고 바꾼다. */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MusicCrossfade = 1.5f;

	//~ 믹스

	/** 볼륨 조절이 얹히는 믹스. 게임 시작 때 한 번 Push된다. */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundMix> MainMix;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundClass> MasterClass;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundClass> MusicClass;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundClass> SfxClass;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundClass> UiClass;

	/** 볼륨 슬라이더를 움직였을 때 실제 볼륨이 따라가는 시간(초). 0이면 즉시. */
	UPROPERTY(Config, EditAnywhere, Category = "Mix", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float VolumeFadeTime = 0.1f;
};
