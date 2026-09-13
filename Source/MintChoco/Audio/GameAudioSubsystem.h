#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameAudioSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundBank;
struct FSoundEvent;

/**
 * 게임의 모든 소리가 지나가는 창구.
 *
 * 호출부는 태그 하나만 넘긴다. 어떤 파일이 어떤 볼륨·피치·감쇠로 나갈지는 뱅크가 정하고,
 * 데디케이티드 서버에서 아무것도 울리지 않게 막는 것도 여기서 한 번만 한다.
 *
 * GameInstance 수명이라 맵이 바뀌어도 살아남는다. 음악이 로비에서 게임 맵까지 끊기지 않고
 * 이어지는 것이 이 수명을 고른 이유다.
 *
 * 소리는 절대 RPC로 보내지 않는다. 모든 머신이 각자의 연출 훅(OnFired, 복제된 태그,
 * 복제된 액터)에서 같은 이벤트를 로컬로 울린다.
 */
UCLASS()
class MINTCHOCO_API UGameAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 월드 컨텍스트로 서브시스템을 찾는다. 없으면 nullptr(에디터 밖 호출, 정리 중). */
	static UGameAudioSubsystem* Get(const UObject* WorldContextObject);

	//~ 한 줄 호출용 정적 헬퍼. 서브시스템이 없으면 조용히 넘어간다.

	static void Play2D(const UObject* WorldContextObject, const FGameplayTag& Tag, const USoundBank* Override = nullptr);
	static void PlayAt(const UObject* WorldContextObject, const FGameplayTag& Tag, const FVector& Location, const USoundBank* Override = nullptr);
	static UAudioComponent* PlayAttached(const FGameplayTag& Tag, USceneComponent* AttachTo, FName Socket = NAME_None, const USoundBank* Override = nullptr);

	//~ 재생

	/** 위치 없이. UI와 "나에게만 들려야 하는" 피드백. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayEvent2D(const FGameplayTag& Tag, const USoundBank* Override = nullptr);

	/** 월드의 한 점에서. 일회성 효과음 대부분이 여기로 온다. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayEventAtLocation(const FGameplayTag& Tag, const FVector& Location, const USoundBank* Override = nullptr);

	/**
	 * 컴포넌트에 붙여서. 움직이는 것을 따라가야 하거나(대시), 도중에 멈춰야 하는
	 * 루프(차지)에 쓴다. 반환된 컴포넌트를 들고 있다가 Stop을 부르면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	UAudioComponent* PlayEventAttached(const FGameplayTag& Tag, USceneComponent* AttachTo, FName Socket = NAME_None, const USoundBank* Override = nullptr);

	//~ 음악

	/**
	 * 틀고 있던 곡을 페이드 아웃하면서 새 곡을 페이드 인한다. 같은 곡을 다시 요청하면
	 * 아무것도 하지 않는다(단계가 여러 번 갱신돼도 곡이 처음부터 다시 시작하지 않는다).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void PlayMusic(const FGameplayTag& Track, float FadeTime = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void StopMusic(float FadeTime = -1.0f);

	/** 지금 틀고 있는 곡. 없으면 빈 태그. */
	UFUNCTION(BlueprintPure, Category = "Audio|Music")
	FGameplayTag GetCurrentMusic() const { return CurrentMusic; }

	/** 유저 세팅의 볼륨을 믹스에 얹는다. 슬라이더가 움직일 때마다 불린다. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void ApplyVolumeSettings();

private:
	/** 오버라이드 뱅크 → 기본 뱅크 순으로 찾는다. 어느 쪽에도 없으면 nullptr. */
	const FSoundEvent* ResolveEvent(const FGameplayTag& Tag, const USoundBank* Override) const;

	/** 데디케이티드 서버이거나 뱅크가 없으면 재생하지 않는다. */
	bool CanPlay() const;

	/** 기본 뱅크. 설정의 소프트 참조를 처음 쓸 때 한 번 로드한다. */
	UPROPERTY(Transient)
	TObjectPtr<const USoundBank> LoadedBank;

	/** 새 맵이 올라온 뒤 믹스를 얹는다. Initialize 시점에는 쓸 만한 월드가 없다. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 지금 흐르는 곡. 맵 전환을 넘기려고 서브시스템이 직접 들고 있다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	/**
	 * 페이드 아웃 중인 이전 곡. 한 컴포넌트로는 크로스페이드가 안 된다 —
	 * UAudioComponent::PlayInternal이 새 사운드를 틀기 전에 옛 것을 Stop하기 때문이다.
	 * 게다가 이 컴포넌트의 Outer는 트랜지언트 패키지라(맵을 넘기려고 그렇게 만든다),
	 * UPROPERTY로 잡아 두지 않으면 페이드가 끝나기 전에 GC가 가져간다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FadingMusicComponent;

	FDelegateHandle PostLoadMapHandle;

	FGameplayTag CurrentMusic;

	bool bMixPushed = false;
};
