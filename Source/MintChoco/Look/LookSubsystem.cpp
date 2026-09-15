#include "Look/LookSubsystem.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/HitResult.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "TimerManager.h"

#include "Look/LookPreset.h"
#include "Look/LookReviewSetup.h"
#include "Look/LookSettings.h"
#include "MintChoco.h"
#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintSettings.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"
#include "Weapons/PaintProjectile.h"

namespace
{
	/** 스왑 대상을 다시 훑는 간격(초). 리스폰한 유닛과 카메라 페이드가 끝난 유닛을 따라잡는다. */
	constexpr float SwapRescanInterval = 0.5f;

	/** 리뷰 스플랫의 입사 속도(cm/s). 반지름이 속도에 조금 비례한다(UPaintBrushProfile::ComputeRadius). */
	constexpr float ReviewSplatSpeed = 1500.0f;

	const FName SkyDomeTopColor(TEXT("TopColor"));
	const FName SkyDomeBottomColor(TEXT("BottomColor"));
	const FName SkyDomeBrightness(TEXT("Brightness"));
	const FName SkyDomeGradientPower(TEXT("GradientPower"));

	UDirectionalLightComponent* FindSun(UWorld& World)
	{
		UDirectionalLightComponent* First = nullptr;
		for (TActorIterator<ADirectionalLight> It(&World); It; ++It)
		{
			UDirectionalLightComponent* const Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (!Light)
			{
				continue;
			}
			if (Light->IsUsedAsAtmosphereSunLight())
			{
				return Light;
			}
			if (!First)
			{
				First = Light;
			}
		}
		return First;
	}

	USkyLightComponent* FindSkyLight(UWorld& World)
	{
		for (TActorIterator<ASkyLight> It(&World); It; ++It)
		{
			if (USkyLightComponent* const Light = It->GetLightComponent())
			{
				return Light;
			}
		}
		return nullptr;
	}

	UExponentialHeightFogComponent* FindFog(UWorld& World)
	{
		for (TActorIterator<AExponentialHeightFog> It(&World); It; ++It)
		{
			if (UExponentialHeightFogComponent* const Fog = It->GetComponent())
			{
				return Fog;
			}
		}
		return nullptr;
	}

	/** 설정의 프리셋을 불러 온다. 못 불러 온 자리는 nullptr, 이름은 NAME_None 으로 채워 번호가 밀리지 않게 한다. */
	void LoadPresets(TArray<ULookPreset*>& OutPresets, TArray<FName>& OutNames)
	{
		for (const TSoftObjectPtr<ULookPreset>& Pointer : ULookSettings::Get().Presets)
		{
			ULookPreset* const Preset = Pointer.LoadSynchronous();
			OutPresets.Add(Preset);
			OutNames.Add(Preset ? Preset->ShortName : NAME_None);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GLookCommand(
		TEXT("mc.Look"),
		TEXT("룩 프리셋을 바꾼다. mc.Look <이름|번호|Off>. 이름과 번호는 mc.Look.List 로 본다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULookSubsystem* const Look = World ? World->GetSubsystem<ULookSubsystem>() : nullptr;
			if (!Look)
			{
				UE_LOG(LogMintChoco, Warning, TEXT("mc.Look: 게임 월드(PIE 포함)에서만 쓸 수 있다."));
				return;
			}

			TArray<ULookPreset*> Presets;
			TArray<FName> Names;
			LoadPresets(Presets, Names);

			int32 Index = INDEX_NONE;
			if (Args.Num() < 1 || !LookPreset::ResolveArgument(Args[0], Names, Index))
			{
				UE_LOG(LogMintChoco, Warning, TEXT("Usage: mc.Look <이름|번호|Off>. mc.Look.List 로 목록을 본다."));
				return;
			}
			if (Index != INDEX_NONE && !Presets[Index])
			{
				UE_LOG(LogMintChoco, Warning, TEXT("mc.Look: %d 번 프리셋을 불러오지 못했다."), Index + 1);
				return;
			}
			Look->ApplyPreset(Index == INDEX_NONE ? nullptr : Presets[Index]);
		}));

	FAutoConsoleCommandWithWorldAndArgs GLookListCommand(
		TEXT("mc.Look.List"),
		TEXT("mc.Look 이 받는 프리셋 번호와 이름을 출력한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const ULookSubsystem* const Look = World ? World->GetSubsystem<ULookSubsystem>() : nullptr;
			const ULookPreset* const Active = Look ? Look->GetActivePreset() : nullptr;

			TArray<ULookPreset*> Presets;
			TArray<FName> Names;
			LoadPresets(Presets, Names);

			UE_LOG(LogMintChoco, Display, TEXT("0  Off%s"), Active ? TEXT("") : TEXT("  <- 지금"));
			for (int32 Index = 0; Index < Presets.Num(); ++Index)
			{
				const ULookPreset* const Preset = Presets[Index];
				UE_LOG(LogMintChoco, Display, TEXT("%d  %s  %s%s"),
					Index + 1,
					*Names[Index].ToString(),
					Preset ? *Preset->Description.ToString() : TEXT("(불러오지 못함)"),
					Preset && Preset == Active ? TEXT("  <- 지금") : TEXT(""));
			}
		}));
}

bool FLookConsoleVariableBackup::Set(const FString& Name, const FString& Value)
{
	IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!Variable)
	{
		return false;
	}
	if (!Originals.Contains(Name))
	{
		Originals.Add(Name, Variable->GetString());
	}
	Variable->Set(*Value, ECVF_SetByCode);
	return true;
}

void FLookConsoleVariableBackup::RestoreAll()
{
	for (const TPair<FString, FString>& Entry : Originals)
	{
		if (IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(*Entry.Key))
		{
			Variable->Set(*Entry.Value, ECVF_SetByCode);
		}
	}
	Originals.Reset();
}

bool LookPreset::ResolveArgument(const FString& Argument, const TArray<FName>& ShortNames, int32& OutIndex)
{
	const FString Trimmed = Argument.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}
	if (Trimmed.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("Baseline"), ESearchCase::IgnoreCase))
	{
		OutIndex = INDEX_NONE;
		return true;
	}
	if (Trimmed.IsNumeric())
	{
		const int32 Number = FCString::Atoi(*Trimmed);
		if (Number == 0)
		{
			OutIndex = INDEX_NONE;
			return true;
		}
		if (Number >= 1 && Number <= ShortNames.Num())
		{
			OutIndex = Number - 1;
			return true;
		}
		return false;
	}
	for (int32 Index = 0; Index < ShortNames.Num(); ++Index)
	{
		if (!ShortNames[Index].IsNone() && Trimmed.Equals(ShortNames[Index].ToString(), ESearchCase::IgnoreCase))
		{
			OutIndex = Index;
			return true;
		}
	}
	return false;
}

bool ULookSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULookSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const ULookSettings& Settings = ULookSettings::Get();
	if (ULookPreset* const Preset = Settings.ReviewPreset.LoadSynchronous())
	{
		ApplyPreset(Preset);
	}
	if (ULookReviewSetup* const Setup = Settings.ReviewSetup.LoadSynchronous())
	{
		StartReview(*Setup);
	}
}

void ULookSubsystem::Deinitialize()
{
	RestoreState(/*bWorldEnding=*/true);
	Super::Deinitialize();
}

void ULookSubsystem::ApplyPreset(ULookPreset* Preset)
{
	RestoreState(/*bWorldEnding=*/false);

	UWorld* const World = GetWorld();
	if (!Preset || !World)
	{
		UE_LOG(LogMintChoco, Log, TEXT("룩 프리셋 해제: 레벨 원래 룩으로 돌아갔다."));
		return;
	}

	ActivePreset = Preset;
	SpawnVolume(*World, *Preset);
	ApplySun(*World, Preset->Sun);
	ApplySkyLight(*World, Preset->SkyLight);
	ApplyFog(*World, Preset->Fog);
	if (Preset->bHideVolumetricCloud)
	{
		HideClouds(*World);
	}
	ApplySkyDome(*World, Preset->SkyDome);
	for (const FLookConsoleVariable& Entry : Preset->ConsoleVariables)
	{
		if (!ConsoleVariables.Set(Entry.Name, Entry.Value))
		{
			UE_LOG(LogMintChoco, Warning, TEXT("룩 프리셋 %s: 콘솔 변수 %s 가 없다."), *Preset->GetName(), *Entry.Name);
		}
	}
	ApplyTeamLook(*World, Preset->TeamLookValues);
	ApplyMaterialSwaps(*World, Preset->MaterialSwaps);

	UE_LOG(LogMintChoco, Log, TEXT("룩 프리셋 %s 를 얹었다."), *Preset->ShortName.ToString());
}

void ULookSubsystem::Restore()
{
	RestoreState(/*bWorldEnding=*/false);
}

void ULookSubsystem::RestoreState(bool bWorldEnding)
{
	ConsoleVariables.RestoreAll();

	if (!bWorldEnding)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SwapRescanTimer);
		}
		if (IsValid(Volume))
		{
			Volume->Destroy();
		}

		if (UDirectionalLightComponent* const Light = SunState.Light.Get())
		{
			Light->SetIntensity(SunState.Intensity);
			Light->SetTemperature(SunState.Temperature);
			Light->SetLightSourceAngle(SunState.SourceAngle);
			Light->SetShadowAmount(SunState.ShadowAmount);
			Light->SetSpecularScale(SunState.SpecularScale);
		}
		if (USkyLightComponent* const Light = SkyLightState.Light.Get())
		{
			Light->SetIntensity(SkyLightState.Intensity);
			Light->SetLightColor(SkyLightState.LightColor);
			Light->SetLowerHemisphereColor(SkyLightState.LowerHemisphereColor);
		}
		if (UExponentialHeightFogComponent* const Fog = FogState.Fog.Get())
		{
			Fog->SetFogDensity(FogState.Density);
			Fog->SetFogInscatteringColor(FogState.InscatteringColor);
			Fog->SetStartDistance(FogState.StartDistance);
		}
		for (const TWeakObjectPtr<USceneComponent>& Cloud : HiddenClouds)
		{
			if (USceneComponent* const Component = Cloud.Get())
			{
				Component->SetVisibility(true);
			}
		}
		if (UMaterialInterface* const Original = DomeMaterial.Get())
		{
			for (const TPair<TWeakObjectPtr<UStaticMeshComponent>, int32>& Slot : DomeSlots)
			{
				if (UStaticMeshComponent* const Mesh = Slot.Key.Get())
				{
					Mesh->SetMaterial(Slot.Value, Original);
				}
			}
		}
		if (UMaterialParameterCollectionInstance* const Instance = TeamLookInstance.Get())
		{
			for (const TPair<FName, FLinearColor>& Entry : TeamLookOriginals)
			{
				Instance->SetVectorParameterValue(Entry.Key, Entry.Value);
			}
		}
		for (const FLookSwappedSlot& Swapped : SwappedSlots)
		{
			UMeshComponent* const Mesh = Swapped.Mesh.Get();
			if (Mesh && Mesh->GetMaterial(Swapped.Slot) == Swapped.Replacement)
			{
				Mesh->SetMaterial(Swapped.Slot, Swapped.Original);
			}
		}
	}

	Volume = nullptr;
	SunState = FLookSunState();
	SkyLightState = FLookSkyLightState();
	FogState = FLookFogState();
	HiddenClouds.Reset();
	DomeSlots.Reset();
	DomeMaterial.Reset();
	TeamLookInstance.Reset();
	TeamLookOriginals.Reset();
	SwapTable.Reset();
	SwappedSlots.Reset();
	ActivePreset = nullptr;
}

void ULookSubsystem::SpawnVolume(UWorld& World, const ULookPreset& Preset)
{
	APostProcessVolume* const NewVolume = World.SpawnActorDeferred<APostProcessVolume>(
		APostProcessVolume::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewVolume)
	{
		return;
	}
	NewVolume->SetFlags(RF_Transient);
	NewVolume->bUnbound = true;
	NewVolume->bEnabled = true;
	NewVolume->BlendWeight = 1.0f;
	NewVolume->Priority = ULookSettings::Get().PostProcessPriority;
	NewVolume->Settings = Preset.PostProcess;
	NewVolume->FinishSpawning(FTransform::Identity);
	Volume = NewVolume;
}

void ULookSubsystem::ApplySun(UWorld& World, const FLookSunSettings& Settings)
{
	UDirectionalLightComponent* const Light = FindSun(World);
	if (!Light)
	{
		return;
	}
	SunState.Light = Light;
	SunState.Intensity = Light->Intensity;
	SunState.Temperature = Light->Temperature;
	SunState.SourceAngle = Light->LightSourceAngle;
	SunState.ShadowAmount = Light->ShadowAmount;
	SunState.SpecularScale = Light->SpecularScale;

	if (Settings.bOverrideIntensity)
	{
		Light->SetIntensity(Settings.Intensity);
	}
	if (Settings.bOverrideTemperature)
	{
		Light->SetTemperature(Settings.Temperature);
	}
	if (Settings.bOverrideSourceAngle)
	{
		Light->SetLightSourceAngle(Settings.SourceAngle);
	}
	if (Settings.bOverrideShadowAmount)
	{
		Light->SetShadowAmount(Settings.ShadowAmount);
	}
	if (Settings.bOverrideSpecularScale)
	{
		Light->SetSpecularScale(Settings.SpecularScale);
	}
}

void ULookSubsystem::ApplySkyLight(UWorld& World, const FLookSkyLightSettings& Settings)
{
	USkyLightComponent* const Light = FindSkyLight(World);
	if (!Light)
	{
		return;
	}
	SkyLightState.Light = Light;
	SkyLightState.Intensity = Light->Intensity;
	SkyLightState.LightColor = FLinearColor(Light->LightColor);
	SkyLightState.LowerHemisphereColor = Light->LowerHemisphereColor;

	if (Settings.bOverrideIntensity)
	{
		Light->SetIntensity(Settings.Intensity);
	}
	if (Settings.bOverrideLightColor)
	{
		Light->SetLightColor(Settings.LightColor);
	}
	if (Settings.bOverrideLowerHemisphereColor)
	{
		Light->SetLowerHemisphereColor(Settings.LowerHemisphereColor);
	}
}

void ULookSubsystem::ApplyFog(UWorld& World, const FLookFogSettings& Settings)
{
	UExponentialHeightFogComponent* const Fog = FindFog(World);
	if (!Fog)
	{
		return;
	}
	FogState.Fog = Fog;
	FogState.Density = Fog->FogDensity;
	FogState.InscatteringColor = Fog->FogInscatteringLuminance;
	FogState.StartDistance = Fog->StartDistance;

	if (Settings.bOverrideDensity)
	{
		Fog->SetFogDensity(Settings.Density);
	}
	if (Settings.bOverrideInscatteringColor)
	{
		Fog->SetFogInscatteringColor(Settings.InscatteringColor);
	}
	if (Settings.bOverrideStartDistance)
	{
		Fog->SetStartDistance(Settings.StartDistance);
	}
}

void ULookSubsystem::HideClouds(UWorld& World)
{
	for (TActorIterator<AVolumetricCloud> It(&World); It; ++It)
	{
		It->ForEachComponent<UVolumetricCloudComponent>(false, [this](UVolumetricCloudComponent* Cloud)
		{
			if (Cloud->IsVisible())
			{
				Cloud->SetVisibility(false);
				HiddenClouds.Add(Cloud);
			}
		});
	}
}

void ULookSubsystem::ApplySkyDome(UWorld& World, const FLookSkyDomeSettings& Settings)
{
	const bool bAnyOverride = Settings.bOverrideTopColor || Settings.bOverrideBottomColor || Settings.bOverrideBrightness || Settings.bOverrideGradientPower;
	UMaterialInterface* const Material = bAnyOverride ? ULookSettings::Get().SkyDomeMaterial.LoadSynchronous() : nullptr;
	if (!Material)
	{
		return;
	}
	DomeMaterial = Material;

	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		It->ForEachComponent<UStaticMeshComponent>(false, [this, &Settings, Material](UStaticMeshComponent* Mesh)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				if (Mesh->GetMaterial(Slot) != Material)
				{
					continue;
				}
				UMaterialInstanceDynamic* const Dynamic = Mesh->CreateDynamicMaterialInstance(Slot, Material);
				if (!Dynamic)
				{
					continue;
				}
				if (Settings.bOverrideTopColor)
				{
					Dynamic->SetVectorParameterValue(SkyDomeTopColor, Settings.TopColor);
				}
				if (Settings.bOverrideBottomColor)
				{
					Dynamic->SetVectorParameterValue(SkyDomeBottomColor, Settings.BottomColor);
				}
				if (Settings.bOverrideBrightness)
				{
					Dynamic->SetScalarParameterValue(SkyDomeBrightness, Settings.Brightness);
				}
				if (Settings.bOverrideGradientPower)
				{
					Dynamic->SetScalarParameterValue(SkyDomeGradientPower, Settings.GradientPower);
				}
				DomeSlots.Emplace(Mesh, Slot);
			}
		});
	}
}

void ULookSubsystem::ApplyTeamLook(UWorld& World, const TArray<FLookCollectionValue>& Values)
{
	if (Values.IsEmpty())
	{
		return;
	}
	UMaterialParameterCollection* const Collection = UPaintSettings::Get().TeamLookCollection.LoadSynchronous();
	UMaterialParameterCollectionInstance* const Instance = Collection ? World.GetParameterCollectionInstance(Collection) : nullptr;
	if (!Instance)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("룩 프리셋: MPC_TeamLook 인스턴스가 없어 팀 룩 값을 건너뛴다."));
		return;
	}
	TeamLookInstance = Instance;

	for (const FLookCollectionValue& Entry : Values)
	{
		FLinearColor Original;
		if (!Instance->GetVectorParameterValue(Entry.ParameterName, Original))
		{
			UE_LOG(LogMintChoco, Warning, TEXT("룩 프리셋: MPC_TeamLook 에 %s 항목이 없다."), *Entry.ParameterName.ToString());
			continue;
		}
		if (!TeamLookOriginals.Contains(Entry.ParameterName))
		{
			TeamLookOriginals.Add(Entry.ParameterName, Original);
		}
		Instance->SetVectorParameterValue(Entry.ParameterName, Entry.Value);
	}
}

void ULookSubsystem::ApplyMaterialSwaps(UWorld& World, const TArray<FLookMaterialSwap>& Swaps)
{
	for (const FLookMaterialSwap& Entry : Swaps)
	{
		UMaterialInterface* const From = Entry.From.LoadSynchronous();
		UMaterialInterface* const To = Entry.To.LoadSynchronous();
		if (!From || !To)
		{
			UE_LOG(LogMintChoco, Warning, TEXT("룩 프리셋: 머티리얼 스왑 %s -> %s 를 불러오지 못했다."), *Entry.From.ToString(), *Entry.To.ToString());
			continue;
		}
		SwapTable.Add(From, To);
	}
	if (SwapTable.IsEmpty())
	{
		return;
	}
	RescanSwaps();
	World.GetTimerManager().SetTimer(SwapRescanTimer, FTimerDelegate::CreateUObject(this, &ULookSubsystem::RescanSwaps), SwapRescanInterval, true);
}

void ULookSubsystem::RescanSwaps()
{
	UWorld* const World = GetWorld();
	if (!World || SwapTable.IsEmpty())
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		It->ForEachComponent<UMeshComponent>(false, [this](UMeshComponent* Mesh)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* const Current = Mesh->GetMaterial(Slot);
				const TObjectPtr<UMaterialInterface>* const Replacement = Current ? SwapTable.Find(Current) : nullptr;
				if (!Replacement)
				{
					continue;
				}
				Mesh->SetMaterial(Slot, *Replacement);
				FLookSwappedSlot& Swapped = SwappedSlots.AddDefaulted_GetRef();
				Swapped.Mesh = Mesh;
				Swapped.Slot = Slot;
				Swapped.Original = Current;
				Swapped.Replacement = *Replacement;
			}
		});
	}
}

void ULookSubsystem::StartReview(ULookReviewSetup& Setup)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	ReviewSetup = &Setup;

	for (const FLookReviewMannequin& Entry : Setup.Mannequins)
	{
		USkeletalMesh* const Mesh = Entry.Mesh.LoadSynchronous();
		if (!Mesh)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* const Mannequin = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), Entry.Transform, Params);
		if (!Mannequin)
		{
			continue;
		}
		USkeletalMeshComponent* const Component = Mannequin->GetSkeletalMeshComponent();
		Component->SetSkeletalMeshAsset(Mesh);
		if (UAnimationAsset* const Animation = Entry.Animation.LoadSynchronous())
		{
			Component->PlayAnimation(Animation, /*bLooping=*/true);
		}
	}
	RescanSwaps();

	if (!Setup.Splats.IsEmpty())
	{
		World->GetTimerManager().SetTimer(ReviewSplatTimer, FTimerDelegate::CreateUObject(this, &ULookSubsystem::StampReviewSplats), FMath::Max(Setup.SplatDelay, 0.01f), false);
	}
}

void ULookSubsystem::StampReviewSplats()
{
	UWorld* const World = GetWorld();
	UPaintSubsystem* const Paint = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
	const UPaintBrushProfile* const Brush = ReviewSetup ? ReviewSetup->Brush.LoadSynchronous() : nullptr;
	if (!Paint || !Brush || !Brush->BrushMaterial)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("리뷰 스플랫: 페인트 서브시스템이나 브러시가 없어 찍지 않는다."));
		return;
	}

	int32 Stamped = 0;
	for (const FLookReviewSplat& Entry : ReviewSetup->Splats)
	{
		FHitResult Hit;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(LookReviewSplat), /*bInTraceComplex=*/true);
		if (!World->LineTraceSingleByChannel(Hit, Entry.Start, Entry.End, PaintballChannel, Query))
		{
			UE_LOG(LogMintChoco, Warning, TEXT("리뷰 스플랫이 아무것도 맞히지 못했다: %s -> %s"), *Entry.Start.ToString(), *Entry.End.ToString());
			continue;
		}
		const FVector Velocity = (Entry.End - Entry.Start).GetSafeNormal() * ReviewSplatSpeed;
		Paint->ApplySplat(Brush->BuildSplat(Hit, Velocity, static_cast<uint8>(Entry.PaintId), Entry.Volume, Entry.HeightAdd, Entry.Seed));
		++Stamped;
	}
	UE_LOG(LogMintChoco, Log, TEXT("리뷰 스플랫 %d/%d 개를 찍었다."), Stamped, ReviewSetup->Splats.Num());
}
