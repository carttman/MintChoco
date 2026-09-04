#include "Sample/SamplePaintController.h"

#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Paint/PaintBrushProfile.h"
#include "Paint/PaintLog.h"
#include "Paint/PaintSplat.h"
#include "Paint/PaintSubsystem.h"
#include "Paint/PaintableComponent.h"
#include "Sample/SampleCoverageWidget.h"
#include "Sample/SampleSeedWidget.h"

ASamplePaintController::ASamplePaintController()
{
	bShowMouseCursor = false;
	SeedWidgetClass = USampleSeedWidget::StaticClass();
	CoverageWidgetClass = USampleCoverageWidget::StaticClass();
}

void ASamplePaintController::BeginPlay()
{
	Super::BeginPlay();

	// Deliberately no SetInputMode call. With bShowMouseCursor false the game viewport already
	// captures the mouse; forcing an input mode here fights the viewport's own focus handling in
	// PIE, which shows up as the click releasing mouse capture.

	NextSeed = FMath::Rand();

	CrosshairWidget = AddLocalWidget(CrosshairWidgetClass);
	SeedWidget = AddLocalWidget(SeedWidgetClass);
	CoverageWidget = AddLocalWidget(CoverageWidgetClass);
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
	UE_CLOG(!PaintMappingContext, LogPaint, Warning, TEXT("%s: PaintMappingContext is unset."), *GetName());
	UE_CLOG(!PaintAction, LogPaint, Warning, TEXT("%s: PaintAction is unset."), *GetName());
	UE_CLOG(!ContinuousPaintAction, LogPaint, Warning, TEXT("%s: ContinuousPaintAction is unset."), *GetName());
	UE_CLOG(!CycleTeamAction, LogPaint, Warning, TEXT("%s: CycleTeamAction is unset."), *GetName());
	UE_CLOG(!BrushProfile, LogPaint, Warning, TEXT("%s: BrushProfile is unset, clicks will not paint."), *GetName());

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

	// Typing into the seed box needs a cursor and UI focus, which the paint viewport otherwise
	// owns. Tab flips between the two on demand instead of forcing an input mode at startup.
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ASamplePaintController::OnToggleUIFocus);
}

void ASamplePaintController::OnToggleUIFocus()
{
	bUIFocused = !bUIFocused;
	bShowMouseCursor = bUIFocused;
	if (bUIFocused)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ASamplePaintController::SetSeedOverride(bool bInUseFixedSeed, int32 InFixedSeed)
{
	bUseFixedSeed = bInUseFixedSeed;
	// A splat carries 16 bits of seed, so a typed value is kept in that range where the box can show it.
	NextSeed = bInUseFixedSeed ? FMath::Clamp(InFixedSeed, 0, static_cast<int32>(MAX_uint16)) : FMath::Rand();
	if (SeedWidget)
	{
		SeedWidget->SetDisplayedSeed(NextSeed);
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

	if (PaintAtHit(Hit, Direction, HeightPerSplat) && bDrawDebugTrace)
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

	if (PaintAtHit(Hit, Direction, HeightPerSplatHeld))
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

	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SamplePaintTrace), /*bTraceComplex=*/false, GetPawn());

	return GetWorld()->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Visibility, Params);
}

bool ASamplePaintController::PaintAtHit(const FHitResult& Hit, const FVector& Direction, float HeightAdd)
{
	UPaintSubsystem* const Paint = GetPaintSubsystem();
	// The trace hits anything; only a hit on a paintable surface is worth a splat.
	const bool bHitPaintable = Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UPaintableComponent>();
	if (!BrushProfile || !Paint || !bHitPaintable)
	{
		return false;
	}

	// A hitscan trace has no speed of its own, so the sample fabricates one. A paintball passes
	// its real impact velocity here instead - that single substitution is the whole difference.
	const FVector IncidentVelocity = Direction * NominalImpactSpeed;

	// The widget always shows the seed the NEXT splat will use: a pinned seed just stays,
	// a free-running one rerolls on every use and the mirror updates with it.
	const FPaintSplat Splat = BrushProfile->BuildSplat(Hit, IncidentVelocity, TeamId, SplatVolume, HeightAdd, NextSeed);
	if (!bUseFixedSeed)
	{
		NextSeed = FMath::Rand();
		if (SeedWidget)
		{
			SeedWidget->SetDisplayedSeed(NextSeed);
		}
	}

	Paint->ApplySplat(Splat);
	return true;
}

void ASamplePaintController::PaintDebugText()
{
	if (UPaintSubsystem* const Paint = GetPaintSubsystem())
	{
		Paint->SetDebugDraw(!Paint->IsAnyDebugTextDrawn(), Paint->AreAnyDebugCellsDrawn());
	}
}

void ASamplePaintController::PaintDebugCells()
{
	if (UPaintSubsystem* const Paint = GetPaintSubsystem())
	{
		Paint->SetDebugDraw(Paint->IsAnyDebugTextDrawn(), !Paint->AreAnyDebugCellsDrawn());
	}
}

void ASamplePaintController::PaintCoverage()
{
	const UPaintSubsystem* const Paint = GetPaintSubsystem();
	if (!Paint)
	{
		return;
	}

	for (const UPaintableComponent* const Paintable : Paint->GetPaintables())
	{
		const FPaintCoverage Surface = Paintable->GetCoverage();
		UE_LOG(LogPaint, Log, TEXT("%s: %s (%.0f cm^2)"), *Paintable->GetReadableName(), *Surface.ToString(), Surface.TotalArea);
	}

	const FPaintCoverage World = Paint->GetWorldCoverage();
	const FString Summary = FString::Printf(TEXT("World: %s (%.0f cm^2)"), *World.ToString(), World.TotalArea);
	UE_LOG(LogPaint, Log, TEXT("%s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key=*/1, /*TimeToDisplay=*/8.0f, FColor::White, Summary);
	}
}

UPaintSubsystem* ASamplePaintController::GetPaintSubsystem() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetSubsystem<UPaintSubsystem>() : nullptr;
}
