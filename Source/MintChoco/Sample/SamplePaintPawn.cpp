#include "Sample/SamplePaintPawn.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "InputActionValue.h"

ASamplePaintPawn::ASamplePaintPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	// The camera owns pitch, the pawn owns yaw. Roll is left alone.
	Camera->bUsePawnControlRotation = true;

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 1200.0f;
	Movement->Acceleration = 6000.0f;
	Movement->Deceleration = 6000.0f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
}

void ASamplePaintPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: input component is not an EnhancedInputComponent."), *GetName());
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASamplePaintPawn::Move);
	}
	if (MoveVerticalAction)
	{
		EnhancedInput->BindAction(MoveVerticalAction, ETriggerEvent::Triggered, this, &ASamplePaintPawn::MoveVertical);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASamplePaintPawn::Look);
	}
}

void ASamplePaintPawn::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator ControlRotation = GetControlRotation();

	// Forward follows the full view direction so the pawn flies where it is looking; strafing
	// stays horizontal so looking up does not tilt sideways movement.
	const FVector Forward = ControlRotation.Vector();
	const FVector Right = FRotationMatrix(FRotator(0.0f, ControlRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void ASamplePaintPawn::MoveVertical(const FInputActionValue& Value)
{
	AddMovementInput(FVector::UpVector, Value.Get<float>());
}

void ASamplePaintPawn::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	AddControllerYawInput(Axis.X);
	// Raw mouse Y grows downward, so it is negated here rather than in the mapping context.
	AddControllerPitchInput(bInvertLook ? Axis.Y : -Axis.Y);
}
