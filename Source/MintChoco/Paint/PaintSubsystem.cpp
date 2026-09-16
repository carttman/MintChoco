#include "Paint/PaintSubsystem.h"

#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Tasks/Task.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Paint/PaintAtlasBaker.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintPlatformCoverage.h"
#include "Paint/PaintSettings.h"
#include "Paint/PaintSplash.h"
#include "Paint/PaintSplashProfile.h"
#include "Paint/PaintSplatEffect.h"
#include "Paint/PaintableComponent.h"
#include "Screen/ScreenFadeSubsystem.h"

namespace
{
	/**
	 * Takes the style scalars positionally and leaves anything not given at its current value, so
	 * a sweep can change one axis per line. With no arguments it only prints what is set.
	 */
	/** The two MPC_PaintStyle entries: Style packs the look, Style2 carries what did not fit. */
	const FName StylePackedParameter(TEXT("Style"));
	const FName StyleExtraParameter(TEXT("Style2"));

	void PaintStyleCommand(const TArray<FString>& Args, UWorld* World)
	{
		UPaintSubsystem* const Paint = World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
		if (!Paint)
		{
			UE_LOG(LogPaint, Warning, TEXT("mc.Paint.Style: 페인트 서브시스템이 없는 월드다."));
			return;
		}

		FPaintLookStyle Style = Paint->GetLookStyle();
		float* const Fields[] = {
			&Style.CoatScale, &Style.FuzzScale, &Style.RoughnessBias, &Style.Flow, &Style.NormalStrength};
		const int32 Given = FMath::Min(Args.Num(), static_cast<int32>(UE_ARRAY_COUNT(Fields)));
		for (int32 Index = 0; Index < Given; ++Index)
		{
			*Fields[Index] = FCString::Atof(*Args[Index]);
		}
		if (Given > 0)
		{
			Paint->SetLookStyle(Style);
		}
		UE_LOG(LogPaint, Log, TEXT("mc.Paint.Style coat=%.2f fuzz=%.2f rough=%.2f flow=%.2f normal=%.2f"),
			Style.CoatScale, Style.FuzzScale, Style.RoughnessBias, Style.Flow, Style.NormalStrength);
	}

	FAutoConsoleCommandWithWorldAndArgs GPaintStyleCommand(
		TEXT("mc.Paint.Style"),
		TEXT("페인트 표면의 룭 스칼라를 바꿄다: <coat 0..1> <fuzz 0..2> <rough 0..1> <flow 0..1> [normal 0..1]. 인자를 생략하면 현재 값만 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PaintStyleCommand));
}

void UPaintSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!PaintPlatformCoverage::IsEnabled(&InWorld)) return;
	const UPaintSettings& Settings = UPaintSettings::Get();
	UStaticMesh* SourceRamp = Settings.PlatformRampSourceMesh.LoadSynchronous();
	UStaticMesh* Ramp = Settings.PlatformRampMesh.LoadSynchronous();
	UMaterialInterface* Material = Settings.PlatformRampMaterial.LoadSynchronous();
	if (!Ramp || !Material)
	{
		UE_LOG(LogPaint, Error, TEXT("Playable platforms: test ramp mesh/material is missing."));
		return;
	}
	int32 Count = 0;
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (!Mesh || !Mesh->GetStaticMesh() || (Mesh->GetStaticMesh() != Ramp && Mesh->GetStaticMesh() != SourceRamp) ||
			It->FindComponentByClass<UPaintableComponent>()) continue;
		// Only the play-world instance changes; the shared mesh, material and other maps stay untouched.
		Mesh->SetStaticMesh(Ramp);
		Mesh->SetMaterial(0, Material);
		UPaintableComponent* Paintable = NewObject<UPaintableComponent>(*It, NAME_None, RF_Transient);
		It->AddInstanceComponent(Paintable);
		Paintable->RegisterComponent();
		++Count;
	}
	UE_LOG(LogPaint, Log, TEXT("Playable platforms: enabled only in %s; %d test ramps registered."),
		*InWorld.GetOutermost()->GetName(), Count);
}

void UPaintSubsystem::RegisterPaintable(UPaintableComponent* Paintable)
{
	if (Paintable)
	{
		Paintables.AddUnique(Paintable);
	}
}

void UPaintSubsystem::UnregisterPaintable(UPaintableComponent* Paintable)
{
	Paintables.RemoveAll([Paintable](const TWeakObjectPtr<UPaintableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Paintable;
	});
}

void UPaintSubsystem::SubmitSplat(const FPaintSplat& Splat)
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		UE_LOG(LogPaint, Warning, TEXT("a client tried to submit a splat; only the server paints, so it was dropped."));
		return;
	}

	// The locks are decided here, once, and travel with the splat: every machine then skips exactly
	// the texels the authority skipped, however far the star state has moved on by the time it draws.
	FPaintSplat Accepted = Splat;
	Accepted.LockGens = GetLockGens().Pack();

	if (OnSplatSubmitted.IsBound())
	{
		OnSplatSubmitted.Execute(Accepted);
	}
	else
	{
		ApplySplat(Accepted);
	}
}

void UPaintSubsystem::ClearPaint()
{
	for (const auto Paintable : GetPaintables())
	{
		Paintable->ClearPaint();
	}
}

void UPaintSubsystem::SetStarPaint(const FPaintStarShaderState& State)
{
	StarPaint = State;
	for (UPaintableComponent* const Paintable : GetPaintables())
	{
		Paintable->ApplyStarPaint(State);
	}
}

FPaintLockGens UPaintSubsystem::GetLockGens() const
{
	return StarPaint.GetLockGens(GetWorld()->GetTimeSeconds());
}

void UPaintSubsystem::ApplySplat(const FPaintSplat& Splat)
{
	if (Splat.bTransient)
	{
		SpawnSideSplatEffect(Splat);
		return;
	}

	// The one per-splat hook every machine passes (the server directly, a client from the replicated
	// log). A volley lands many in one frame; the bank's concurrency limit keeps that to a few voices.
	// A phantom landing and a droplet's mark are silent: their contact already made its sound.
	if (!Splat.bScoreOnly && !Splat.bDrawOnly)
	{
		UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_World_Splat, Splat.Location);
	}

	StampSurfaces(Splat);
	if (Splat.Splash)
	{
		ApplyPhantomLandings(Splat);
	}
}

void UPaintSubsystem::StampSurfaces(const FPaintSplat& Splat)
{
	// A physics overlap rather than the registry: collision, not a bounding box, decides which
	// surfaces the stamp can reach, and it is the same query a projectile hit came from.
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(
		Overlaps, Splat.Location, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Splat.GetWorldExtent()));

	// Overlap results repeat an actor once per overlapping component, so dedupe on the
	// paintable itself before drawing.
	TSet<UPaintableComponent*> Painted;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* const Actor = Overlap.GetActor();
		UPaintableComponent* const Paintable =
			Actor ? Actor->FindComponentByClass<UPaintableComponent>() : nullptr;
		if (!Paintable)
		{
			continue;
		}

		bool bAlreadyPainted = false;
		Painted.Add(Paintable, &bAlreadyPainted);
		if (!bAlreadyPainted)
		{
			Paintable->ApplySplat(Splat);
		}
	}
}

void UPaintSubsystem::ApplyPhantomLandings(const FPaintSplat& Splat)
{
	const UPaintSplashProfile& Profile = *Splat.Splash;

	PaintSplash::FSpawnInput Input;
	Input.ImpactPoint = Splat.Location;
	Input.ImpactNormal = Splat.Normal;
	Input.IncidentVelocity = FVector(Splat.IncidentDir) * static_cast<double>(Splat.IncidentSpeed);
	Input.BallRadius = Splat.BallRadius > 0 ? static_cast<float>(Splat.BallRadius) : 6.0f;
	Input.Seed = Splat.Seed;

	TArray<PaintSplash::FPhantomLanding> Landings;
	PaintSplash::PhantomLandings(Profile, Input, GetWorld()->GetGravityZ(), Landings);
	for (int32 Index = 0; Index < Landings.Num(); ++Index)
	{
		const PaintSplash::FPhantomLanding& Landing = Landings[Index];
		const float Radius = Profile.ComputeMarkRadius(Landing.Speed) * Profile.PhantomCellRadiusScale;
		if (Radius <= 0.0f)
		{
			continue;
		}

		// A round mark on the contact's own plane, so the parent's frame serves. No splash of its
		// own: a phantom never expands again.
		FPaintSplat Phantom = Splat;
		Phantom.Splash = nullptr;
		Phantom.bScoreOnly = true;
		Phantom.Location = Landing.Point;
		Phantom.Radius = Radius;
		Phantom.Stretch = 1.0f;
		Phantom.ImpactU = 0.0f;
		Phantom.Seed = static_cast<uint16>(HashCombineFast(static_cast<uint32>(Splat.Seed), static_cast<uint32>(Index + 1)) & 0xFFFF);
		StampSurfaces(Phantom);
	}
}

void UPaintSubsystem::SpawnSideSplatEffect(const FPaintSplat& Splat)
{
	UWorld* const World = GetWorld();
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!SideSplatEffectClass)
	{
		SideSplatEffectClass = UPaintSettings::Get().SideSplatEffectClass.LoadSynchronous();
		if (!SideSplatEffectClass)
		{
			UE_LOG(LogPaint, Verbose, TEXT("a transient splat had nothing to show: Project Settings > Game > Paint > Side Splat Effect Class is unset."));
			return;
		}
	}

	// Z along the surface normal, X along the stamp's U axis, nudged off the surface so a decal
	// or a quad placed at the origin does not fight the wall it lands on.
	const FVector Normal(Splat.Normal);
	const FTransform Transform(
		FRotationMatrix::MakeFromZX(Normal, FVector(Splat.AxisU)).ToQuat(),
		FVector(Splat.Location) + Normal * 0.5);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AActor* const Effect = World->SpawnActor(SideSplatEffectClass, &Transform, Params);
	if (Effect && Effect->Implements<UPaintSplatEffect>())
	{
		IPaintSplatEffect::Execute_OnPaintSplat(Effect, Splat);
	}
}

FPaintCoverage UPaintSubsystem::GetWorldCoverage() const
{
	FPaintCoverage Coverage;
	for (const TWeakObjectPtr<UPaintableComponent>& Entry : Paintables)
	{
		if (const UPaintableComponent* const Paintable = Entry.Get())
		{
			Coverage.Add(Paintable->GetCoverage());
		}
	}
	return Coverage;
}

TArray<UPaintableComponent*> UPaintSubsystem::GetPaintables() const
{
	TArray<UPaintableComponent*> Result;
	for (const TWeakObjectPtr<UPaintableComponent>& Entry : Paintables)
	{
		if (UPaintableComponent* const Paintable = Entry.Get())
		{
			Result.Add(Paintable);
		}
	}
	return Result;
}

void UPaintSubsystem::SetLookStyle(const FPaintLookStyle& Style)
{
	LookStyle = Style;

	// A collection reaches every paint material at once and is live without a recompile, which
	// is what makes a look sweep possible at all; a material parameter would only reach the one
	// instance that was written, and a layer parameter cannot be written by name from C++.
	UMaterialParameterCollection* const Collection = UPaintSettings::Get().StyleCollection.LoadSynchronous();
	if (!Collection)
	{
		UE_LOG(LogPaint, Warning, TEXT("StyleCollectionÇ74 Åc6Åb4 ¸ed Âa4Î7c·7c¹7c Äe0 ¬f3Ç74 Åc6²e4."));
		return;
	}
	UWorld* const World = GetWorld();
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, StylePackedParameter,
		FLinearColor(Style.CoatScale, Style.FuzzScale, Style.RoughnessBias, Style.Flow));
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, StyleExtraParameter,
		FLinearColor(Style.NormalStrength, 0.0f, 0.0f, 0.0f));
}

void UPaintSubsystem::RequestAtlas(
	const UStaticMeshComponent& Mesh,
	int32 MaterialSlot,
	const FBox& LocalBounds,
	const FPaintIslandLayout& Layout,
	FPaintAtlasReady OnReady)
{
	const UStaticMesh* const Asset = Mesh.GetStaticMesh();
	if (!Asset || Layout.IsEmpty())
	{
		return;
	}

	const UPaintSettings& Settings = UPaintSettings::Get();
	const FString Key = MakeAtlasKey(*Asset, Layout, Settings.EdgeFadeTexels, Settings.EdgeFadeSeamFraction);
	if (const FPaintAtlas* const Cached = AtlasCache.Find(Key))
	{
		OnReady.ExecuteIfBound(*Cached);
		return;
	}
	if (TArray<FPaintAtlasReady>* const Waiters = PendingAtlases.Find(Key))
	{
		Waiters->Add(MoveTemp(OnReady));
		return;
	}

	const TSharedRef<FPaintAtlasBakeInput> Input = MakeShared<FPaintAtlasBakeInput>();
	if (!PaintAtlasBaker::GatherMesh(Mesh, MaterialSlot, *Input))
	{
		return;
	}
	Input->LocalBounds = LocalBounds;
	Input->Layout = Layout;
	Input->EdgeFadeTexels = Settings.EdgeFadeTexels;
	Input->EdgeFadeSeamFraction = Settings.EdgeFadeSeamFraction;
	PendingAtlases.Add(Key).Add(MoveTemp(OnReady));
	UE_LOG(LogPaint, Log, TEXT("baking a paint atlas for %s: %s"), *Asset->GetName(), *Layout.ToString());

	// The map is not ready to be shown until its surfaces can be painted; the screen fade
	// waits for this. The hold dies with this subsystem, so a bake cut short by travel cannot
	// keep the next map dark.
	if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(this))
	{
		Fade->AddHold(this, FName(*Key));
	}

	// The bake only reads its own copies, so it runs off the game thread; only the textures
	// have to be created back on it.
	TWeakObjectPtr<UPaintSubsystem> WeakThis(this);
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [WeakThis, Key, Input]()
	{
		const TSharedRef<FPaintAtlasBakeOutput> Output = MakeShared<FPaintAtlasBakeOutput>();
		PaintAtlasBaker::Bake(*Input, *Output);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Key, Input, Output]()
		{
			if (UPaintSubsystem* const Self = WeakThis.Get())
			{
				Self->FinishAtlas(Key, Input->Layout, *Output);
			}
		});
	}, UE::Tasks::ETaskPriority::BackgroundNormal);
}

void UPaintSubsystem::FinishAtlas(const FString& Key, const FPaintIslandLayout& Layout, const FPaintAtlasBakeOutput& Output)
{
	TArray<FPaintAtlasReady> Waiters;
	PendingAtlases.RemoveAndCopyValue(Key, Waiters);
	if (UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(this))
	{
		Fade->RemoveHold(this, FName(*Key));
	}

	FPaintAtlas& Atlas = AtlasCache.Add(Key);
	Atlas.Layout = Layout;
	Atlas.PositionMap = PaintAtlasBaker::CreatePositionTexture(Output, Layout.AtlasSize, NAME_None);
	Atlas.EdgeFadeMap = PaintAtlasBaker::CreateEdgeFadeTexture(Output, Layout.AtlasSize, NAME_None);
	UE_LOG(LogPaint, Log, TEXT("paint atlas baked: %d of %d texels covered."),
		Output.CoveredTexels, Layout.AtlasSize * Layout.AtlasSize);

	for (FPaintAtlasReady& Waiter : Waiters)
	{
		Waiter.ExecuteIfBound(Atlas);
	}
}

FString UPaintSubsystem::MakeAtlasKey(const UStaticMesh& Mesh, const FPaintIslandLayout& Layout, float FadeTexels, float SeamFraction)
{
	return FString::Printf(TEXT("%s|%08x|%d|%d"),
		*Mesh.GetPathName(), Layout.ComputeHash(), FMath::RoundToInt(FadeTexels * 100.0f), FMath::RoundToInt(SeamFraction * 1000.0f));
}

UTextureRenderTarget2D* UPaintSubsystem::GetScratchTarget(int32 Size)
{
	if (const TObjectPtr<UTextureRenderTarget2D>* const Found = ScratchTargets.Find(Size))
	{
		return *Found;
	}
	UTextureRenderTarget2D* const Target = CreatePaintBuffer(this, Size);
	ScratchTargets.Add(Size, Target);
	return Target;
}

UTextureRenderTarget2D* UPaintSubsystem::CreatePaintBuffer(UObject* Outer, int32 Size)
{
	// Built by hand instead of CreateRenderTarget2D because the buffer needs its sampler settings
	// fixed before the resource is created: bilinear filtering would invent team ids on every
	// splat boundary, and sRGB would corrupt the id -> byte round trip.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	// B is the distance to the nearest paint edge in texels, encoded as 1 - d / PaintDistanceRange
	// so that a cleared or default texel (B = 0) reads as "far".
	Buffer->RenderTargetFormat = RTF_RGBA8;
	Buffer->ClearColor = PaintIdNoneColor;
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Size, Size);
	Buffer->UpdateResourceImmediate(true);
	return Buffer;
}

void UPaintSubsystem::SetDebugDraw(bool bText, bool bCells)
{
	for (UPaintableComponent* const Paintable : GetPaintables())
	{
		Paintable->SetDebugDraw(bText, bCells);
	}
}

bool UPaintSubsystem::IsAnyDebugTextDrawn() const
{
	return GetPaintables().ContainsByPredicate(
		[](const UPaintableComponent* Paintable) { return Paintable->IsDebugTextDrawn(); });
}

bool UPaintSubsystem::AreAnyDebugCellsDrawn() const
{
	return GetPaintables().ContainsByPredicate(
		[](const UPaintableComponent* Paintable) { return Paintable->AreDebugCellsDrawn(); });
}

void UPaintSubsystem::Deinitialize()
{
	// A bake still running finds this subsystem gone through its weak pointer and drops its result.
	PendingAtlases.Empty();
	AtlasCache.Empty();
	ScratchTargets.Empty();
	Super::Deinitialize();
}

bool UPaintSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
