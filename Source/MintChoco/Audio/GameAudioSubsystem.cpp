#include "Audio/GameAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

#include "Audio/GameAudioSettings.h"
#include "Audio/SoundBank.h"
#include "MintChoco.h"

void UGameAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 데디케이티드 서버는 오디오 디바이스가 없다. 뱅크를 로드할 이유도, 믹스를 얹을 이유도 없다.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	const UGameAudioSettings& Settings = UGameAudioSettings::Get();
	LoadedBank = Settings.Bank.LoadSynchronous();
	if (!LoadedBank)
	{
		UE_LOG(LogMintChoco, Warning,
			TEXT("사운드 뱅크가 설정되지 않았습니다. 게임은 조용히 돕니다(Project Settings > Game Audio)."));
	}

	// 믹스는 여기서 얹지 않는다. GameInstance의 Initialize 시점에는 쓸 만한 월드가 없다 —
	// 스탠드얼론에서는 버려질 DummyWorld가 현재 월드로 잡혀 있어서, 그 월드에 얹은 믹스는
	// 아무 데도 닿지 않는다. 맵이 올라온 뒤에 한다(ScreenFadeSubsystem과 같은 이유).
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGameAudioSubsystem::HandlePostLoadMap);
}

void UGameAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	ApplyVolumeSettings();
}

void UGameAudioSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (MusicComponent)
	{
		MusicComponent->Stop();
		MusicComponent = nullptr;
	}
	if (FadingMusicComponent)
	{
		FadingMusicComponent->Stop();
		FadingMusicComponent = nullptr;
	}
	CurrentMusic = FGameplayTag();

	Super::Deinitialize();
}

UGameAudioSubsystem* UGameAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameAudioSubsystem>() : nullptr;
}

void UGameAudioSubsystem::Play2D(const UObject* WorldContextObject, const FGameplayTag& Tag, const USoundBank* Override)
{
	if (UGameAudioSubsystem* const Audio = Get(WorldContextObject))
	{
		Audio->PlayEvent2D(Tag, Override);
	}
}

void UGameAudioSubsystem::PlayAt(const UObject* WorldContextObject, const FGameplayTag& Tag, const FVector& Location, const USoundBank* Override)
{
	if (UGameAudioSubsystem* const Audio = Get(WorldContextObject))
	{
		Audio->PlayEventAtLocation(Tag, Location, Override);
	}
}

UAudioComponent* UGameAudioSubsystem::PlayAttached(const FGameplayTag& Tag, USceneComponent* AttachTo, FName Socket, const USoundBank* Override)
{
	UGameAudioSubsystem* const Audio = AttachTo ? Get(AttachTo) : nullptr;
	return Audio ? Audio->PlayEventAttached(Tag, AttachTo, Socket, Override) : nullptr;
}

bool UGameAudioSubsystem::CanPlay() const
{
	return !IsRunningDedicatedServer();
}

const FSoundEvent* UGameAudioSubsystem::ResolveEvent(const FGameplayTag& Tag, const USoundBank* Override) const
{
	// 개체별 뱅크가 먼저다. 오버라이드에는 바꿀 항목만 들어 있으므로, 없으면 기본 뱅크로 내려간다.
	if (Override)
	{
		if (const FSoundEvent* const Found = Override->Find(Tag))
		{
			return Found;
		}
	}
	return LoadedBank ? LoadedBank->Find(Tag) : nullptr;
}

namespace
{
	/** 이벤트의 피치 범위에서 한 번 뽑는다. 두 값이 같으면 랜덤 없이 그 값이다. */
	float PickPitch(const FSoundEvent& Event)
	{
		return FMath::IsNearlyEqual(Event.PitchRange.X, Event.PitchRange.Y)
			? Event.PitchRange.X
			: FMath::FRandRange(Event.PitchRange.X, Event.PitchRange.Y);
	}
}

void UGameAudioSubsystem::PlayEvent2D(const FGameplayTag& Tag, const USoundBank* Override)
{
	if (!CanPlay())
	{
		return;
	}

	const FSoundEvent* const Event = ResolveEvent(Tag, Override);
	if (!Event || !Event->Sound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Event->Sound, Event->VolumeMultiplier, PickPitch(*Event), 0.0f, Event->Concurrency);
}

void UGameAudioSubsystem::PlayEventAtLocation(const FGameplayTag& Tag, const FVector& Location, const USoundBank* Override)
{
	if (!CanPlay())
	{
		return;
	}

	const FSoundEvent* const Event = ResolveEvent(Tag, Override);
	if (!Event || !Event->Sound)
	{
		return;
	}

	// 2D로 표시된 이벤트는 위치를 무시한다. 위치를 넘긴 쪽이 아니라 뱅크가 정한다.
	if (Event->b2D)
	{
		PlayEvent2D(Tag, Override);
		return;
	}

	USoundAttenuation* const Attenuation = Event->Attenuation
		? Event->Attenuation.Get()
		: UGameAudioSettings::Get().DefaultAttenuation.LoadSynchronous();

	UGameplayStatics::PlaySoundAtLocation(
		this, Event->Sound, Location, FRotator::ZeroRotator,
		Event->VolumeMultiplier, PickPitch(*Event), 0.0f, Attenuation, Event->Concurrency);
}

UAudioComponent* UGameAudioSubsystem::PlayEventAttached(const FGameplayTag& Tag, USceneComponent* AttachTo, FName Socket, const USoundBank* Override)
{
	if (!CanPlay() || !AttachTo)
	{
		return nullptr;
	}

	const FSoundEvent* const Event = ResolveEvent(Tag, Override);
	if (!Event || !Event->Sound)
	{
		return nullptr;
	}

	USoundAttenuation* const Attenuation = Event->Attenuation
		? Event->Attenuation.Get()
		: UGameAudioSettings::Get().DefaultAttenuation.LoadSynchronous();

	return UGameplayStatics::SpawnSoundAttached(
		Event->Sound, AttachTo, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, /*bStopWhenAttachedToDestroyed=*/true,
		Event->VolumeMultiplier, PickPitch(*Event), 0.0f, Attenuation, Event->Concurrency);
}

void UGameAudioSubsystem::PlayMusic(const FGameplayTag& Track, float FadeTime)
{
	if (!CanPlay() || CurrentMusic == Track)
	{
		return;
	}

	const UGameAudioSettings& Settings = UGameAudioSettings::Get();
	const float Fade = FadeTime >= 0.0f ? FadeTime : Settings.MusicCrossfade;

	const FSoundEvent* const Event = ResolveEvent(Track, nullptr);
	if (!Event || !Event->Sound)
	{
		StopMusic(Fade);
		return;
	}

	// 앞선 크로스페이드가 아직 안 끝났으면 그쪽은 여기서 잘라낸다. 슬롯은 하나뿐이라
	// 곡을 연달아 바꿔도 페이드 중인 컴포넌트가 쌓이지 않는다.
	if (FadingMusicComponent)
	{
		FadingMusicComponent->Stop();
	}

	FadingMusicComponent = MusicComponent;
	if (FadingMusicComponent)
	{
		FadingMusicComponent->FadeOut(Fade, 0.0f);
	}

	// bPersistAcrossLevelTransition: 맵이 바뀌어도 이 컴포넌트가 살아남는다. 곡이 로비에서
	// 게임 맵까지 이어지는 것은 이 인자 하나에 달려 있다. bAutoDestroy는 꺼야 다음 곡으로
	// 갈아탈 때까지 우리가 들고 있을 수 있다.
	//
	// Spawn이 아니라 Create인 이유: Spawn은 즉시 전체 볼륨으로 재생을 시작해 버려서,
	// 뒤이어 FadeIn을 부르면 곡이 처음부터 다시 시작한다. Create로 만들어 두고 FadeIn이
	// 재생을 시작하게 한다.
	MusicComponent = UGameplayStatics::CreateSound2D(
		this, Event->Sound, Event->VolumeMultiplier, PickPitch(*Event), 0.0f, Event->Concurrency,
		/*bPersistAcrossLevelTransition=*/true, /*bAutoDestroy=*/false);

	if (MusicComponent)
	{
		if (Fade > 0.0f)
		{
			MusicComponent->FadeIn(Fade, Event->VolumeMultiplier);
		}
		else
		{
			MusicComponent->Play();
		}
	}

	CurrentMusic = Track;
}

void UGameAudioSubsystem::StopMusic(float FadeTime)
{
	if (!MusicComponent)
	{
		CurrentMusic = FGameplayTag();
		return;
	}

	const float Fade = FadeTime >= 0.0f ? FadeTime : UGameAudioSettings::Get().MusicCrossfade;
	if (Fade > 0.0f)
	{
		MusicComponent->FadeOut(Fade, 0.0f);
	}
	else
	{
		MusicComponent->Stop();
	}

	MusicComponent = nullptr;
	CurrentMusic = FGameplayTag();
}

void UGameAudioSubsystem::ApplyVolumeSettings()
{
	if (!CanPlay())
	{
		return;
	}

	const UGameAudioSettings& Settings = UGameAudioSettings::Get();
	USoundMix* const Mix = Settings.MainMix.LoadSynchronous();
	if (!Mix)
	{
		return;
	}

	// 믹스는 한 번만 얹는다. 이후 볼륨 변경은 클래스별 오버라이드로 덮어쓴다.
	if (!bMixPushed)
	{
		UGameplayStatics::PushSoundMixModifier(this, Mix);
		bMixPushed = true;
	}

	// TODO(볼륨 옵션 단계): UMintChocoUserSettings에서 값을 읽어 클래스마다
	// SetSoundMixClassOverride를 건다. 유저 세팅 클래스가 생기기 전까지는 믹스만 얹어 둔다.
}
