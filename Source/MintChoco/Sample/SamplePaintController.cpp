#include "Sample/SamplePaintController.h"

#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintableComponent.h"

ASamplePaintController::ASamplePaintController()
{
	bShowMouseCursor = false;
}

void ASamplePaintController::BeginPlay()
{
	Super::BeginPlay();

	// Deliberately no SetInputMode call. With bShowMouseCursor false the game viewport already
	// captures the mouse; forcing an input mode here fights the viewport's own focus handling in
	// PIE, which shows up as the click releasing mouse capture.

	if (IsLocalPlayerController() && CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(this, CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport();
		}
	}
}

void ASamplePaintController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (PaintMappingContext)
		{
			Subsystem->AddMappingContext(PaintMappingContext, 0);
		}
	}

	// Every one of these is an asset reference set on the Blueprint, and an unset one fails
	// silently as "the button does nothing" - which is expensive to diagnose from the symptom.
	UE_CLOG(!PaintMappingContext, LogTemp, Warning, TEXT("%s: PaintMappingContext is unset."), *GetName());
	UE_CLOG(!PaintAction, LogTemp, Warning, TEXT("%s: PaintAction is unset."), *GetName());
	UE_CLOG(!ContinuousPaintAction, LogTemp, Warning, TEXT("%s: ContinuousPaintAction is unset."), *GetName());
	UE_CLOG(!CycleTeamAction, LogTemp, Warning, TEXT("%s: CycleTeamAction is unset."), *GetName());

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (PaintAction)
		{
			EnhancedInput->BindAction(
				PaintAction, ETriggerEvent::Started, this, &ASamplePaintController::OnPaintTriggered);
		}
		if (ContinuousPaintAction)
		{
			EnhancedInput->BindAction(
				ContinuousPaintAction, ETriggerEvent::Triggered, this,
				&ASamplePaintController::OnContinuousPaintTriggered);
			EnhancedInput->BindAction(
				ContinuousPaintAction, ETriggerEvent::Completed, this,
				&ASamplePaintController::OnContinuousPaintReleased);
			EnhancedInput->BindAction(
				ContinuousPaintAction, ETriggerEvent::Canceled, this,
				&ASamplePaintController::OnContinuousPaintReleased);
		}
		if (CycleTeamAction)
		{
			EnhancedInput->BindAction(
				CycleTeamAction, ETriggerEvent::Triggered, this,
				&ASamplePaintController::OnCycleTeamTriggered);
		}
	}
}

void ASamplePaintController::OnCycleTeamTriggered(const FInputActionValue& Value)
{
	const int32 Step = Value.Get<float>() > 0.0f ? 1 : -1;
	const int32 TeamCount = FMath::Max(static_cast<int32>(NumTeams), 1);
	TeamId = static_cast<uint8>((TeamId + Step + TeamCount) % TeamCount);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			/*Key=*/0, /*TimeToDisplay=*/2.0f, FColor::White,
			FString::Printf(TEXT("Paint team: %d"), TeamId));
	}
}

void ASamplePaintController::OnPaintTriggered()
{
	FHitResult Hit;
	FVector Direction;
	if (!TracePaintTarget(Hit, Direction))
	{
		return;
	}

	if (PaintAtHit(Hit, Direction) && bDrawDebugTrace)
	{
		DrawDebugLine(GetWorld(), Hit.TraceStart, Hit.ImpactPoint, FColor::Cyan, false, 2.0f);
		DrawDebugDirectionalArrow(
			GetWorld(), Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 50.0f,
			10.0f, FColor::Yellow, false, 2.0f);
	}
}

void ASamplePaintController::OnContinuousPaintTriggered()
{
	FHitResult Hit;
	FVector Direction;
	if (!TracePaintTarget(Hit, Direction))
	{
		// Dropping the anchor here means sweeping off a surface and back on starts a fresh stroke
		// rather than one that jumps the gap.
		bStrokeAnchorValid = false;
		return;
	}

	if (bStrokeAnchorValid
		&& FVector::DistSquared(Hit.ImpactPoint, StrokeAnchor) < FMath::Square(ContinuousSpacing))
	{
		return;
	}

	if (PaintAtHit(Hit, Direction))
	{
		StrokeAnchor = Hit.ImpactPoint;
		bStrokeAnchorValid = true;
	}
}

void ASamplePaintController::OnContinuousPaintReleased()
{
	bStrokeAnchorValid = false;
}

bool ASamplePaintController::TracePaintTarget(FHitResult& OutHit, FVector& OutDirection) const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	OutDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + OutDirection * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SamplePaintTrace), /*bTraceComplex=*/true, GetPawn());
	// Both of these are required for FindCollisionUV to return anything but (0,0). The project
	// also needs Physics -> Support UV From Hit Results enabled.
	Params.bReturnFaceIndex = true;

	return GetWorld()->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Visibility, Params);
}

bool ASamplePaintController::PaintAtHit(const FHitResult& Hit, const FVector& Direction)
{
	UPaintableComponent* Paintable = Hit.GetActor()
		? Hit.GetActor()->FindComponentByClass<UPaintableComponent>()
		: nullptr;
	if (!Paintable)
	{
		return false;
	}

	// A hitscan trace has no speed of its own, so the sample fabricates one. A paintball passes
	// its real impact velocity here instead - that single substitution is the whole difference.
	const FVector IncidentVelocity = Direction * NominalImpactSpeed;

	FPaintSplat Splat;
	if (!Paintable->BuildSplatFromHit(Hit, IncidentVelocity, TeamId, SplatVolume, Splat))
	{
		return false;
	}

	Paintable->ApplySplat(Splat);

	return true;
}
