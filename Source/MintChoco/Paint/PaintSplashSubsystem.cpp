#include "Paint/PaintSplashSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"

#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintSplash.h"
#include "Paint/PaintSplashProfile.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"

static TAutoConsoleVariable<int32> CVarPaintSplashMarks(
	TEXT("mc.PaintSplash.Marks"),
	1,
	TEXT("1이면 스플래시 방울이 착지한 자리에 자국을 찍는다. 0이면 방울만 날고 자국은 남지 않는다(점수는 그대로)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarPaintSplashDebug(
	TEXT("mc.PaintSplash.Debug"),
	0,
	TEXT("1이면 스플래시마다 착탄점과 방울이 닿을 수 있는 반경을 그리고, 착지마다 로그를 남긴다."),
	ECVF_Default);

namespace
{
	/** Splashes whose droplets may be in the air at once; beyond it a contact shows droplets but leaves no marks. */
	constexpr int32 MaxHandlers = 256;
}

void UPaintSplashLandingHandler::Arm(const FPaintSplashRequest& Request, UPaintSubsystem* InPaint, uint32 InLockGens, float InExpiresAt)
{
	Profile = Request.Profile;
	Paint = InPaint;
	ImpactPoint = Request.ImpactPoint;
	PaintId = Request.PaintId;
	LockGens = InLockGens;
	Seed = PaintSplash::SplashSeed(Request.Seed);
	ExpiresAt = InExpiresAt;
	Landings = 0;
	bArmed = true;
}

void UPaintSplashLandingHandler::Release()
{
	Profile = nullptr;
	Paint = nullptr;
	bArmed = false;
}

void UPaintSplashLandingHandler::ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data, UNiagaraSystem*, const FVector& SimulationPositionOffset)
{
	UPaintSubsystem* const PaintSystem = Paint.Get();
	const UPaintBrushProfile* const Brush = bArmed && Profile ? Profile->DropletBrush.Get() : nullptr;
	if (!PaintSystem || !Brush || !Brush->BrushMaterial)
	{
		return;
	}

	for (const FBasicParticleData& Landing : Data)
	{
		const FVector Normal = Landing.Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		FHitResult Hit;
		Hit.ImpactPoint = Landing.Position + SimulationPositionOffset;
		Hit.Location = Hit.ImpactPoint;
		Hit.ImpactNormal = Normal;
		Hit.Normal = Normal;

		// Head-on at the launch speed: a round mark sized like the phantom the score claimed for it.
		++Landings;
		const FVector Incident = -Normal * FMath::Max(Landing.Size, 0.0f);
		const int32 MarkSeed = static_cast<int32>(HashCombineFast(static_cast<uint32>(Seed), static_cast<uint32>(Landings)));
		FPaintSplat Splat = Brush->BuildSplat(Hit, Incident, PaintId, Profile->DropletSplatVolume, Profile->DropletHeightAdd, MarkSeed);
		Splat.LockGens = LockGens;
		Splat.bDrawOnly = true;
		PaintSystem->ApplySplat(Splat);

		if (CVarPaintSplashDebug.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogPaint, Log, TEXT("splash: droplet landed at %s, mark radius %.1f cm."), *Hit.ImpactPoint.ToString(), Splat.Radius);
		}
	}
}

UPaintSplashSubsystem* UPaintSplashSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UPaintSplashSubsystem>() : nullptr;
}

bool UPaintSplashSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPaintSplashSubsystem::DrawsMarks() const
{
	const UWorld* const World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer && CVarPaintSplashMarks.GetValueOnGameThread() != 0;
}

UPaintSplashLandingHandler* UPaintSplashSubsystem::AcquireHandler(float Now)
{
	for (UPaintSplashLandingHandler* const Handler : Handlers)
	{
		if (Handler->HasExpired(Now))
		{
			Handler->Release();
			return Handler;
		}
	}
	if (Handlers.Num() >= MaxHandlers)
	{
		return nullptr;
	}
	return Handlers.Add_GetRef(NewObject<UPaintSplashLandingHandler>(this));
}

UPaintSplashLandingHandler* UPaintSplashSubsystem::BeginSplash(const FPaintSplashRequest& Request)
{
	UWorld* const World = GetWorld();
	if (!World || !Request.Profile || !Request.bLeavesMarks || !DrawsMarks())
	{
		return nullptr;
	}
	const UPaintBrushProfile* const Brush = Request.Profile->DropletBrush;
	if (!Brush || !Brush->BrushMaterial)
	{
		return nullptr;
	}

	const float Now = World->GetTimeSeconds();
	UPaintSplashLandingHandler* const Handler = AcquireHandler(Now);
	if (!Handler)
	{
		UE_LOG(LogPaint, Verbose, TEXT("splash: every landing handler is busy, this contact leaves no marks."));
		return nullptr;
	}

	UPaintSubsystem* const Paint = World->GetSubsystem<UPaintSubsystem>();
	Handler->Arm(Request, Paint, Paint ? Paint->GetLockGens().Pack() : 0, Now + Request.Profile->GetHoldSeconds());

#if ENABLE_DRAW_DEBUG
	if (CVarPaintSplashDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(World, Request.ImpactPoint, Request.Profile->MaxTravel, 16, FColor::Cyan, false, Request.Profile->GetHoldSeconds());
	}
#endif
	return Handler;
}

UMaterialInstanceDynamic* UPaintSplashSubsystem::BuildBlobMaterial(UNiagaraComponent& Effect, const UPaintSplashProfile& Profile, const FPaintSplashRequest& Request,
	TArrayView<const PaintSplash::FDroplet> Droplets, const FVector& BlobScale, float GravityZ)
{
	if (!Profile.BlobMaterial)
	{
		return nullptr;
	}

	// Outered to the pooled component, so the instance lives exactly as long as the component holds it.
	UMaterialInstanceDynamic* const Blob = UMaterialInstanceDynamic::Create(Profile.BlobMaterial, &Effect);
	Blob->SetScalarParameterValue(PaintSplashBlob::TeamId, static_cast<float>(Request.PaintId));
	for (int32 Index = 0; Index < PaintSplash::MaxDroplets; ++Index)
	{
		const bool bUsed = Index < Droplets.Num();
		const FVector Offset = bUsed ? Droplets[Index].Position - Request.ImpactPoint : FVector::ZeroVector;
		const FVector Velocity = bUsed ? Droplets[Index].Velocity : FVector::ZeroVector;
		Blob->SetVectorParameterValue(PaintSplashBlob::Drop[Index], FLinearColor(Offset.X, Offset.Y, Offset.Z, bUsed ? Droplets[Index].Radius : 0.0f));
		Blob->SetVectorParameterValue(PaintSplashBlob::Velocity[Index], FLinearColor(Velocity.X, Velocity.Y, Velocity.Z, 0.0f));
	}
	Blob->SetVectorParameterValue(PaintSplashBlob::Physics, FLinearColor(-GravityZ * Profile.GravityScale, Profile.Drag, Profile.CohesionRadius, Profile.CohesionDecay));
	Blob->SetVectorParameterValue(PaintSplashBlob::Crown,
		FLinearColor(0.5f * Request.BallRadius, Profile.CrownRadiusScale * Request.BallRadius, Profile.CrownThicknessScale * Request.BallRadius, Profile.CrownLifetime));
	Blob->SetScalarParameterValue(PaintSplashBlob::MarchMax, static_cast<float>(BlobScale.Size() * 100.0));
	return Blob;
}

void UPaintSplashSubsystem::ConfigureEffect(UNiagaraComponent& Effect, const FPaintSplashRequest& Request, UPaintSplashLandingHandler* Handler)
{
	const UPaintSplashProfile* const Profile = Request.Profile;
	PaintSplash::FSpawnInput Input;
	Input.ImpactPoint = Request.ImpactPoint;
	Input.ImpactNormal = Request.ImpactNormal;
	Input.IncidentVelocity = Request.IncidentVelocity;
	Input.BallRadius = Request.BallRadius;
	Input.Seed = Request.Seed;
	TArray<PaintSplash::FDroplet> Droplets;
	if (Profile)
	{
		PaintSplash::GenerateDroplets(*Profile, Input, Droplets);
	}

	Effect.SetVariableInt(PaintSplashFX::DropletCount, Droplets.Num());
	if (Droplets.IsEmpty())
	{
		Effect.SetVariableObject(PaintSplashFX::LandingHandler, nullptr);
		return;
	}

	for (int32 Index = 0; Index < PaintSplash::MaxDroplets; ++Index)
	{
		const bool bUsed = Index < Droplets.Num();
		Effect.SetVariableVec3(PaintSplashFX::DropOffset[Index], bUsed ? Droplets[Index].Position - Request.ImpactPoint : FVector::ZeroVector);
		Effect.SetVariableVec3(PaintSplashFX::DropVelocity[Index], bUsed ? Droplets[Index].Velocity : FVector::ZeroVector);
		Effect.SetVariableFloat(PaintSplashFX::DropRadius[Index], bUsed ? Droplets[Index].Radius : 0.0f);
	}
	Effect.SetVariableFloat(PaintSplashFX::BallRadius, Request.BallRadius);
	Effect.SetVariableInt(PaintSplashFX::Seed, PaintSplash::SplashSeed(Request.Seed));

	const struct
	{
		const FName& Name;
		float Value;
	} Floats[] = {
		{PaintSplashFX::GravityScale, Profile->GravityScale},
		{PaintSplashFX::Drag, Profile->Drag},
		{PaintSplashFX::MaxLifetime, Profile->MaxLifetime},
		{PaintSplashFX::MaxTravel, Profile->MaxTravel},
		{PaintSplashFX::CohesionRadius, Profile->CohesionRadius},
		{PaintSplashFX::CohesionDecay, Profile->CohesionDecay},
		{PaintSplashFX::CrownRadiusScale, Profile->CrownRadiusScale},
		{PaintSplashFX::CrownThicknessScale, Profile->CrownThicknessScale},
		{PaintSplashFX::CrownLifetime, Profile->CrownLifetime},
	};
	for (const auto& Entry : Floats)
	{
		Effect.SetVariableFloat(Entry.Name, Entry.Value);
	}

	const float GravityZ = GetWorld()->GetGravityZ();
	const FVector BlobScale = PaintSplash::BlobScale(*Profile, Input, Droplets, GravityZ);
	Effect.SetVariableVec3(PaintSplashFX::BlobScale, BlobScale);
	Effect.SetVariableMaterial(PaintSplashFX::BlobMaterial, BuildBlobMaterial(Effect, *Profile, Request, Droplets, BlobScale, GravityZ));

	Effect.SetVariableObject(PaintSplashFX::LandingHandler, Handler);
	if (Handler)
	{
		Effect.OnSystemFinished.AddUniqueDynamic(this, &UPaintSplashSubsystem::HandleEffectFinished);
		Active.Add(&Effect, Handler);
	}
}

void UPaintSplashSubsystem::HandleEffectFinished(UNiagaraComponent* Effect)
{
	TWeakObjectPtr<UPaintSplashLandingHandler> Handler;
	if (Active.RemoveAndCopyValue(Effect, Handler) && Handler.IsValid())
	{
		Handler->Release();
	}
}

int32 UPaintSplashSubsystem::GetArmedHandlerCount() const
{
	int32 Count = 0;
	for (const UPaintSplashLandingHandler* const Handler : Handlers)
	{
		Count += Handler->IsArmed() ? 1 : 0;
	}
	return Count;
}

void UPaintSplashSubsystem::Deinitialize()
{
	Active.Empty();
	Handlers.Empty();
	Super::Deinitialize();
}
