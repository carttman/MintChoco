#include "Paint/PaintMapBaker.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PipelineStateCache.h"
#include "RHI.h"
#include "TextureResource.h"
#include "TimerManager.h"

#include "Paint/PaintLog.h"
#include "Paint/PaintSettings.h"

namespace
{
	const FName UnwrapOriginParam(TEXT("UnwrapOrigin"));
	const FName UnwrapSizeParam(TEXT("UnwrapSize"));
	const FName PositionMapParam(TEXT("PositionMap"));
	const FName FadeTexelsParam(TEXT("FadeTexels"));
	const FName SeamFractionParam(TEXT("SeamFraction"));

	constexpr float BakePollInterval = 0.1f;
	constexpr float MaxBakeWaitSeconds = 3.0f;
	constexpr float MaxBakePollSeconds = 30.0f;
	constexpr float BakeRetryInterval = 0.5f;
}

void UPaintMapBaker::Initialize(
	UStaticMeshComponent* InSourceMesh,
	int32 InResolution,
	float InPlaneSize,
	float InFadeTexels,
	float InSeamFraction)
{
	SourceMesh = InSourceMesh;
	Resolution = InResolution;
	PlaneSize = InPlaneSize;
	FadeTexels = InFadeTexels;
	SeamFraction = InSeamFraction;
}

void UPaintMapBaker::RequestBake()
{
	UMaterialInterface* const UnwrapMaterial = UPaintSettings::Get().UnwrapMaterial.LoadSynchronous();
	if (!SourceMesh || !UnwrapMaterial)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: baking needs a source mesh and the Unwrap Material from Project Settings > Game > Paint."), *GetName());
		return;
	}

	if (!ProxyMesh)
	{
		UnwrapMID = UMaterialInstanceDynamic::Create(UnwrapMaterial, this);

		// Registering the proxy with the unwrap material already applied files the PSO precache
		// request through the engine's normal path - no draw call needed to trigger it.
		ProxyMesh = NewObject<UStaticMeshComponent>(SourceMesh->GetOwner());
		ProxyMesh->SetStaticMesh(SourceMesh->GetStaticMesh());
		ProxyMesh->bVisibleInSceneCaptureOnly = true;
		// The unwrap WPO must flatten the raw vertex mesh; a Nanite-enabled source asset
		// would route the proxy through the Nanite raster path instead.
		ProxyMesh->SetForceDisableNanite(true);
		ProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProxyMesh->RegisterComponent();
		ProxyMesh->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		for (int32 Slot = 0; Slot < ProxyMesh->GetNumMaterials(); ++Slot)
		{
			ProxyMesh->SetMaterial(Slot, UnwrapMID);
		}
	}

	BakeWaitElapsed = 0.0f;
	LastAttemptElapsed = -BakeRetryInterval;
	GetWorld()->GetTimerManager().SetTimer(
		BakeWaitHandle, this, &UPaintMapBaker::TryBake, BakePollInterval, true);
}

void UPaintMapBaker::TryBake()
{
	BakeWaitElapsed += BakePollInterval;

	if (BakeWaitElapsed >= MaxBakePollSeconds)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: gave up waiting for a valid position map after %.0fs."), *GetName(), MaxBakePollSeconds);
		GetWorld()->GetTimerManager().ClearTimer(BakeWaitHandle);
		return;
	}

	// A busy precache queue is a cheap reason to wait, but an idle queue proves nothing:
	// on the very first poll our own PSO request may not even be filed yet. Ground truth
	// is the bake result itself - bake, read the pixels back, retry until they are real.
	if (PipelineStateCache::NumActivePrecacheRequests() > 0 && BakeWaitElapsed < MaxBakeWaitSeconds)
	{
		return;
	}
	if (BakeWaitElapsed - LastAttemptElapsed < BakeRetryInterval)
	{
		return;
	}
	LastAttemptElapsed = BakeWaitElapsed;

	Bake();
	if (IsPositionMapValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(BakeWaitHandle);
		BakeEdgeFade();
		OnBaked.ExecuteIfBound(PositionRenderTarget, EdgeFadeRenderTarget);
	}
}

bool UPaintMapBaker::IsPositionMapValid() const
{
	if (!PositionRenderTarget)
	{
		return false;
	}
	FTextureRenderTargetResource* const Resource = PositionRenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return false;
	}

	// One row through the middle is enough: any reasonable unwrap layout crosses the center
	// line, while a capture that missed the mesh entirely is uniformly black. Synchronous
	// readback is fine here - this runs a handful of times at load, never per frame.
	TArray<FLinearColor> Pixels;
	const int32 Row = Resolution / 2;
	if (!Resource->ReadLinearColorPixels(
			Pixels, FReadSurfaceDataFlags(RCM_MinMax), FIntRect(0, Row, Resolution, Row + 1)))
	{
		return false;
	}
	for (const FLinearColor& Pixel : Pixels)
	{
		if (Pixel.R > UE_KINDA_SMALL_NUMBER || Pixel.G > UE_KINDA_SMALL_NUMBER || Pixel.B > UE_KINDA_SMALL_NUMBER)
		{
			return true;
		}
	}
	return false;
}

void UPaintMapBaker::Bake()
{
	if (!ProxyMesh || !UnwrapMID)
	{
		return;
	}

	EnsureCaptureComponent();

	const FVector UnwrapOrigin = ProxyMesh->GetOwner()->GetActorLocation();
	UnwrapMID->SetVectorParameterValue(UnwrapOriginParam, FLinearColor(UnwrapOrigin));
	UnwrapMID->SetScalarParameterValue(UnwrapSizeParam, PlaneSize);

	constexpr float CaptureHeight = 500.0f;
	Capture->SetWorldLocationAndRotation(
		UnwrapOrigin + FVector(0.0f, 0.0f, CaptureHeight), FRotator(-90.0f, 0.0f, 0.0f));
	Capture->CaptureScene();
}

void UPaintMapBaker::BakeEdgeFade()
{
	UMaterialInterface* const EdgeFadeMaterial = UPaintSettings::Get().EdgeFadeMaterial.LoadSynchronous();
	if (!EdgeFadeMaterial)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: Edge Fade Material is unset in Project Settings > Game > Paint, displacement will tear at island edges."), *GetName());
		return;
	}

	if (!EdgeFadeRenderTarget)
	{
		EdgeFadeRenderTarget = CreateFadeBuffer(this, Resolution);
	}
	if (!EdgeFadeMID)
	{
		EdgeFadeMID = UMaterialInstanceDynamic::Create(EdgeFadeMaterial, this);
	}

	EdgeFadeMID->SetTextureParameterValue(PositionMapParam, PositionRenderTarget);
	EdgeFadeMID->SetScalarParameterValue(FadeTexelsParam, FadeTexels);
	EdgeFadeMID->SetScalarParameterValue(SeamFractionParam, SeamFraction);

	// The map only depends on the unwrap, never on the paint, so one draw per bake is enough.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, EdgeFadeRenderTarget, EdgeFadeMID);
}

void UPaintMapBaker::EnsurePositionBuffer()
{
	if (!PositionRenderTarget)
	{
		PositionRenderTarget = CreatePositionBuffer(this, Resolution);
	}
}

void UPaintMapBaker::EnsureCaptureComponent()
{
	// The capture is born pointing at the buffer, so the buffer has to exist first.
	EnsurePositionBuffer();

	if (!Capture)
	{
		Capture = CreateCaptureComponent(ProxyMesh, PlaneSize, PositionRenderTarget);
	}
}

UTextureRenderTarget2D* UPaintMapBaker::CreatePositionBuffer(UObject* Outer, int32 Resolution)
{
	// Nearest for the same reason as the id buffer: bilinear across a UV island boundary would
	// blend two unrelated surface positions into one that exists nowhere on the mesh.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	Buffer->RenderTargetFormat = RTF_RGBA16f;
	Buffer->ClearColor = FLinearColor::Black;
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Resolution, Resolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

UTextureRenderTarget2D* UPaintMapBaker::CreateFadeBuffer(UObject* Outer, int32 Resolution)
{
	// A plain scalar, so unlike the id buffer it can be filtered by the sampler. White means
	// "no fade", which is also what the surface material assumes until the bake delivers.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(Outer);
	Buffer->RenderTargetFormat = RTF_R8;
	Buffer->ClearColor = FLinearColor::White;
	Buffer->Filter = TF_Bilinear;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Resolution, Resolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

USceneCaptureComponent2D* UPaintMapBaker::CreateCaptureComponent(
	UStaticMeshComponent* ProxyMesh, float OrthoWidth, UTextureRenderTarget2D* Target)
{
	// The unwrap plane sits at the owner: frustum culling still uses the original primitive
	// bounds, so a faraway plane would cull the mesh and capture nothing. ShowOnlyComponents
	// keeps neighbouring geometry out instead.
	const auto Capture = NewObject<USceneCaptureComponent2D>(ProxyMesh->GetOwner());
	Capture->RegisterComponent();
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = OrthoWidth;
	Capture->TextureTarget = Target;
	Capture->CaptureSource = SCS_SceneColorHDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponents.Add(ProxyMesh);

	return Capture;
}

void UPaintMapBaker::Shutdown()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BakeWaitHandle);
	}
	if (Capture)
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	if (ProxyMesh)
	{
		ProxyMesh->DestroyComponent();
		ProxyMesh = nullptr;
	}
	UnwrapMID = nullptr;
	EdgeFadeMID = nullptr;
	PositionRenderTarget = nullptr;
	EdgeFadeRenderTarget = nullptr;
}
