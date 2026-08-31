#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "SamplePaintPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class UInputAction;
struct FInputActionValue;

/** Free-flying camera for the sample map. Enhanced Input only - no legacy axis bindings. */
UCLASS()
class MINTCHOCO_API ASamplePaintPawn : public APawn
{
	GENERATED_BODY()

public:
	ASamplePaintPawn();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void MoveVertical(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sample")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sample")
	TObjectPtr<UFloatingPawnMovement> Movement;

	/** Axis2D: X is right, Y is forward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Axis1D: positive is up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> MoveVerticalAction;

	/** Axis2D: raw mouse delta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Input")
	bool bInvertLook = false;
};
