#include "Paint/PositionMapBaker.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PipelineStateCache.h"
#include "TimerManager.h"

namespace
{
	const FName UnwrapOriginParam(TEXT("UnwrapOrigin"));
	const FName UnwrapSizeParam(TEXT("UnwrapSize"));

	constexpr float BakePollInterval = 0.1f;
	constexpr float MaxBakeWaitSeconds = 3.0f;
}

void UPositionMapBaker::Initialize(
	UMeshComponent* InSourceMesh,
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
	UStaticMeshComponent* const StaticSource = Cast<UStaticMeshComponent>(SourceMesh);
	if (!StaticSource || !UnwrapMaterial)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: baking needs a StaticMeshComponent source and an unwrap material."), *GetName());
		return;
	}

	if (!ProxyMesh)
	{
		UnwrapMID = UMaterialInstanceDynamic::Create(UnwrapMaterial, this);

		// Registering the proxy with the unwrap material already applied files the PSO precache
		// request through the engine's normal path - no draw call needed to trigger it.
		ProxyMesh = NewObject<UStaticMeshComponent>(StaticSource->GetOwner());
		ProxyMesh->SetStaticMesh(StaticSource->GetStaticMesh());
		ProxyMesh->bVisibleInSceneCaptureOnly = true;
		ProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProxyMesh->RegisterComponent();
		ProxyMesh->AttachToComponent(StaticSource, FAttachmentTransformRules::SnapToTargetIncludingScale);
		for (int32 Slot = 0; Slot < ProxyMesh->GetNumMaterials(); ++Slot)
		{
			ProxyMesh->SetMaterial(Slot, UnwrapMID);
		}
	}

	BakeWaitElapsed = 0.0f;
	GetWorld()->GetTimerManager().SetTimer(
		BakeWaitHandle, this, &UPositionMapBaker::TryBake, BakePollInterval, true);
}

void UPositionMapBaker::TryBake()
{
	BakeWaitElapsed += BakePollInterval;

	const bool bPrecacheIdle = PipelineStateCache::NumActivePrecacheRequests() == 0;
	if (!bPrecacheIdle && BakeWaitElapsed < MaxBakeWaitSeconds)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(BakeWaitHandle);
	Bake();
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
	UnwrapMID->SetVectorParameterValue(
		UnwrapOriginParam, FLinearColor(UnwrapOrigin.X, UnwrapOrigin.Y, UnwrapOrigin.Z));
	UnwrapMID->SetScalarParameterValue(UnwrapSizeParam, PlaneSize);

	// The unwrap plane sits at the owner: frustum culling still uses the original primitive
	// bounds, so a faraway plane would cull the mesh and capture nothing. ShowOnlyComponents
	// keeps neighbouring geometry out instead.
	USceneCaptureComponent2D* const Capture = NewObject<USceneCaptureComponent2D>(ProxyMesh->GetOwner());
	Capture->RegisterComponent();
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = PlaneSize;
	Capture->TextureTarget = PositionRenderTarget;
	Capture->CaptureSource = SCS_SceneColorHDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponents.Add(ProxyMesh);

	constexpr float CaptureHeight = 500.0f;
	Capture->SetWorldLocationAndRotation(
		UnwrapOrigin + FVector(0.0f, 0.0f, CaptureHeight), FRotator(-90.0f, 0.0f, 0.0f));
	Capture->CaptureScene();
	Capture->DestroyComponent();

	OnBaked.ExecuteIfBound(PositionRenderTarget);
}

UTextureRenderTarget2D* UPositionMapBaker::CreatePositionBuffer()
{
	// Nearest for the same reason as the id buffer: bilinear across a UV island boundary would
	// blend two unrelated surface positions into one that exists nowhere on the mesh.
	UTextureRenderTarget2D* const Buffer = NewObject<UTextureRenderTarget2D>(this);
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
	if (ProxyMesh)
	{
		ProxyMesh->DestroyComponent();
		ProxyMesh = nullptr;
	}
	UnwrapMID = nullptr;
	PositionRenderTarget = nullptr;
}
