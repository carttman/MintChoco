#include "Paint/PositionMapBaker.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PipelineStateCache.h"
#include "RHI.h"
#include "TextureResource.h"
#include "TimerManager.h"

namespace
{
	const FName UnwrapOriginParam(TEXT("UnwrapOrigin"));
	const FName UnwrapSizeParam(TEXT("UnwrapSize"));

	constexpr float BakePollInterval = 0.1f;
	constexpr float MaxBakeWaitSeconds = 3.0f;
	constexpr float MaxBakePollSeconds = 30.0f;
	constexpr float BakeRetryInterval = 0.5f;
}

void UPositionMapBaker::Initialize(
	UStaticMeshComponent* InSourceMesh,
	UMaterialInterface* InUnwrapMaterial,
	int32 InResolution,
	float InPlaneSize)
{
	SourceMesh = InSourceMesh;
	UnwrapMaterial = InUnwrapMaterial;
	Resolution = InResolution;
	PlaneSize = InPlaneSize;
}

void UPositionMapBaker::RequestBake()
{
	if (!SourceMesh || !UnwrapMaterial)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: baking needs a source mesh and an unwrap material."), *GetName());
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
		BakeWaitHandle, this, &UPositionMapBaker::TryBake, BakePollInterval, true);
}

void UPositionMapBaker::TryBake()
{
	BakeWaitElapsed += BakePollInterval;

	if (BakeWaitElapsed >= MaxBakePollSeconds)
	{
		UE_LOG(LogTemp, Warning,
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
		OnBaked.ExecuteIfBound(PositionRenderTarget);
	}
}

bool UPositionMapBaker::IsPositionMapValid() const
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

void UPositionMapBaker::Bake()
{
	if (!ProxyMesh || !UnwrapMID)
	{
		return;
	}

	if (!PositionRenderTarget)
	{
		PositionRenderTarget = CreatePositionBuffer();
	}

	const FVector UnwrapOrigin = ProxyMesh->GetOwner()->GetActorLocation();
	UnwrapMID->SetVectorParameterValue(UnwrapOriginParam, FLinearColor(UnwrapOrigin));
	UnwrapMID->SetScalarParameterValue(UnwrapSizeParam, PlaneSize);

	if (!Capture)
	{
		// The unwrap plane sits at the owner: frustum culling still uses the original primitive
		// bounds, so a faraway plane would cull the mesh and capture nothing. ShowOnlyComponents
		// keeps neighbouring geometry out instead.
		Capture = NewObject<USceneCaptureComponent2D>(ProxyMesh->GetOwner());
		Capture->RegisterComponent();
		Capture->ProjectionType = ECameraProjectionMode::Orthographic;
		Capture->OrthoWidth = PlaneSize;
		Capture->TextureTarget = PositionRenderTarget;
		Capture->CaptureSource = SCS_SceneColorHDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyComponents.Add(ProxyMesh);
	}

	constexpr float CaptureHeight = 500.0f;
	Capture->SetWorldLocationAndRotation(
		UnwrapOrigin + FVector(0.0f, 0.0f, CaptureHeight), FRotator(-90.0f, 0.0f, 0.0f));
	Capture->CaptureScene();
}

UTextureRenderTarget2D* UPositionMapBaker::CreatePositionBuffer()
{
	// Nearest for the same reason as the id buffer: bilinear across a UV island boundary would
	// blend two unrelated surface positions into one that exists nowhere on the mesh.
	const auto Buffer = NewObject<UTextureRenderTarget2D>(this);
	Buffer->RenderTargetFormat = RTF_RGBA16f;
	Buffer->ClearColor = FLinearColor::Black;
	Buffer->Filter = TF_Nearest;
	Buffer->SRGB = false;
	Buffer->InitAutoFormat(Resolution, Resolution);
	Buffer->UpdateResourceImmediate(true);

	return Buffer;
}

void UPositionMapBaker::Shutdown()
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
	PositionRenderTarget = nullptr;
}
