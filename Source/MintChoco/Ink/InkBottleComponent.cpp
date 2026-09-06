#include "Ink/InkBottleComponent.h"

#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Ink/InkTankComponent.h"

namespace
{
	const FName FillParam(TEXT("Fill"));
	const FName WobbleTiltParam(TEXT("WobbleTilt"));
	const FName WobbleEnergyParam(TEXT("WobbleEnergy"));
	const FName LiquidHeightParam(TEXT("LiquidHeight"));
	const FName SurfaceUpBlendParam(TEXT("SurfaceUpBlend"));
	const FName BottleCenterParam(TEXT("BottleCenter"));
	const FName BottleUpParam(TEXT("BottleUp"));
	const FName BottleRadiusParam(TEXT("BottleRadius"));

	/** A frame longer than this is a hitch; the spring skips its spike instead of launching off it. */
	constexpr float MaxWobbleStep = 0.1f;

	/** How fast the ripple energy follows the slosh speed. */
	constexpr float EnergyFollowSpeed = 4.0f;

	/** Keeps the surface off the cylinder's end caps, where the disc would z-fight them. */
	constexpr float SurfaceFillMargin = 0.03f;

	/** The disc is scaled past the radius so a tilted surface still reaches the wall; the material clips the rest. */
	constexpr float SurfaceDiscOversize = 1.6f;

	/** The disc stops just inside the wall so its flat edge never pokes through the cylinder. */
	constexpr float SurfaceRadiusInset = 0.99f;
}

UInkBottleComponent::UInkBottleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UInkBottleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (const AActor* const Owner = GetOwner())
	{
		Tank = Owner->FindComponentByClass<UInkTankComponent>();
	}
	DisplayedFill = Tank.IsValid() ? Tank->GetInk() : DefaultFill;

	// A pawn whose team has not arrived yet still shows a bottle; SetTeam swaps the material later.
	if (!LiquidMID)
	{
		RebuildMaterial(GetMaterial(0));
	}
	if (!SurfaceMID && SurfaceMesh)
	{
		RebuildSurfaceMaterial(SurfaceMesh->GetMaterial(0));
	}
}

void UInkBottleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFill(DeltaTime);
	UpdateWobble(DeltaTime);
	PushRuntimeParameters();
	UpdateSurface();
}

void UInkBottleComponent::SetTeam(int32 TeamId)
{
	if (TeamMaterials.IsValidIndex(TeamId) && TeamMaterials[TeamId])
	{
		RebuildMaterial(TeamMaterials[TeamId]);
	}
	if (TeamSurfaceMaterials.IsValidIndex(TeamId) && TeamSurfaceMaterials[TeamId])
	{
		RebuildSurfaceMaterial(TeamSurfaceMaterials[TeamId]);
	}
}

void UInkBottleComponent::SetSurfaceMesh(UStaticMeshComponent* InSurfaceMesh)
{
	SurfaceMesh = InSurfaceMesh;
	if (SurfaceMesh)
	{
		SurfaceMesh->SetUsingAbsoluteLocation(true);
		SurfaceMesh->SetUsingAbsoluteRotation(true);
		SurfaceMesh->SetUsingAbsoluteScale(true);
	}
}

void UInkBottleComponent::UpdateFill(float DeltaTime)
{
	const float Target = Tank.IsValid() ? Tank->GetInk() : DefaultFill;
	DisplayedFill = FMath::FInterpTo(DisplayedFill, Target, DeltaTime, FillSmoothing);
}

void UInkBottleComponent::UpdateWobble(float DeltaTime)
{
	const AActor* const Owner = GetOwner();
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;

	// The owner's velocity rather than a position delta: on a listen server a remote pawn only
	// moves when its ServerMove arrives, so a per-frame delta would spike and stall in turns.
	const FVector OwnerVelocity = Movement ? Movement->Velocity : (Owner ? Owner->GetVelocity() : FVector::ZeroVector);
	const FQuat Rotation = GetComponentQuat();

	FVector Omega = FVector::ZeroVector;
	if (bHasPrevious && DeltaTime > 0.0f)
	{
		FQuat Delta = Rotation * PrevRotation.Inverse();
		if (Delta.W < 0.0f)
		{
			Delta = -Delta;
		}
		FVector Axis;
		float Angle;
		Delta.ToAxisAndAngle(Axis, Angle);
		Omega = Axis * (Angle / DeltaTime);
	}

	// The bottle hangs off the pawn's back, so turning swings it: add the arm's tangential speed.
	const FVector Arm = Owner ? GetComponentLocation() - Owner->GetActorLocation() : FVector::ZeroVector;
	const FVector Velocity = OwnerVelocity + (Omega ^ Arm);

	if (bHasPrevious && DeltaTime > 0.0f && DeltaTime <= MaxWobbleStep)
	{
		const FVector Accel = (Velocity - PrevVelocity) / DeltaTime;

		// The liquid lags the bottle and piles against the wall opposite the acceleration, which
		// leans the surface normal along the acceleration. A spin about a horizontal axis drags it too.
		const FVector2D Force =
			FVector2D(Accel.X, Accel.Y) * LinearGain + FVector2D(-Omega.Y, Omega.X) * AngularGain;
		const FVector2D SpringAccel = Force - Tilt * Stiffness - TiltVelocity * Damping;
		TiltVelocity += SpringAccel * DeltaTime;
		Tilt += TiltVelocity * DeltaTime;
		Tilt = Tilt.ClampAxes(-MaxTilt, MaxTilt);

		const float TargetEnergy = FMath::Min(TiltVelocity.Size() * EnergyGain, 1.0f);
		Energy = FMath::FInterpTo(Energy, TargetEnergy, DeltaTime, EnergyFollowSpeed);
	}

	PrevVelocity = Velocity;
	PrevRotation = Rotation;
	bHasPrevious = true;
}

void UInkBottleComponent::UpdateSurface()
{
	const UStaticMesh* const Mesh = GetStaticMesh();
	if (!SurfaceMesh || !Mesh)
	{
		return;
	}

	// The same plane the liquid material cuts with: through the bottle axis at the fill height,
	// leaning with the slosh. Bounds.Origin is what the material reads as ObjectPositionWS.
	const FVector Extent = Mesh->GetBounds().BoxExtent * GetComponentScale();
	const FVector Up = GetComponentQuat().GetAxisZ();
	const float Fill = FMath::Clamp(DisplayedFill, SurfaceFillMargin, 1.0f - SurfaceFillMargin);
	const FVector Center = Bounds.Origin + Up * ((Fill - 0.5f) * Extent.Z * 2.0f);
	const FVector Normal = ComputeSurfaceNormal();
	SurfaceMesh->SetWorldLocationAndRotation(Center, FRotationMatrix::MakeFromZ(Normal).ToQuat());

	const float Radius = FMath::Max(Extent.X, Extent.Y);
	const UStaticMesh* const DiscMesh = SurfaceMesh->GetStaticMesh();
	const float DiscHalfSize = DiscMesh ? FMath::Max(DiscMesh->GetBounds().BoxExtent.X, 1.0f) : 50.0f;
	const float DiscScale = Radius * SurfaceDiscOversize / DiscHalfSize;
	SurfaceMesh->SetWorldScale3D(FVector(DiscScale, DiscScale, 1.0f));

	if (SurfaceMID)
	{
		SurfaceMID->SetVectorParameterValue(BottleCenterParam, FLinearColor(Bounds.Origin));
		SurfaceMID->SetVectorParameterValue(BottleUpParam, FLinearColor(Up));
		SurfaceMID->SetScalarParameterValue(BottleRadiusParam, Radius * SurfaceRadiusInset);
		SurfaceMID->SetScalarParameterValue(WobbleEnergyParam, Energy);
	}
}

void UInkBottleComponent::RebuildMaterial(UMaterialInterface* Base)
{
	if (!Base || (LiquidMID && LiquidMID->Parent == Base))
	{
		return;
	}

	LiquidMID = UMaterialInstanceDynamic::Create(Base, this);
	SetMaterial(0, LiquidMID);

	LiquidMID->SetScalarParameterValue(LiquidHeightParam, ComputeLiquidHeight());
	if (HorizonLock >= 0.0f)
	{
		LiquidMID->SetScalarParameterValue(SurfaceUpBlendParam, HorizonLock);
	}
	PushRuntimeParameters();
}

void UInkBottleComponent::RebuildSurfaceMaterial(UMaterialInterface* Base)
{
	if (!SurfaceMesh || !Base || (SurfaceMID && SurfaceMID->Parent == Base))
	{
		return;
	}

	SurfaceMID = UMaterialInstanceDynamic::Create(Base, this);
	SurfaceMesh->SetMaterial(0, SurfaceMID);
	UpdateSurface();
}

void UInkBottleComponent::PushRuntimeParameters()
{
	if (!LiquidMID)
	{
		return;
	}

	// The walls get the same clamped fill as the disc, so the cut never lands on an end cap.
	const float Fill = FMath::Clamp(DisplayedFill, SurfaceFillMargin, 1.0f - SurfaceFillMargin);
	LiquidMID->SetScalarParameterValue(FillParam, Fill);
	LiquidMID->SetVectorParameterValue(WobbleTiltParam, FLinearColor(Tilt.X, Tilt.Y, 0.0f, 0.0f));
	LiquidMID->SetScalarParameterValue(WobbleEnergyParam, Energy);
}

float UInkBottleComponent::ComputeLiquidHeight() const
{
	// The material measures the fill along the mesh's local Z, in world centimetres.
	const UStaticMesh* const Mesh = GetStaticMesh();
	return Mesh ? Mesh->GetBounds().BoxExtent.Z * 2.0f * GetComponentScale().Z : 0.0f;
}

float UInkBottleComponent::GetSurfaceUpBlend() const
{
	if (HorizonLock >= 0.0f)
	{
		return HorizonLock;
	}
	float Value = 1.0f;
	if (LiquidMID)
	{
		LiquidMID->GetScalarParameterValue(SurfaceUpBlendParam, Value);
	}
	return Value;
}

FVector UInkBottleComponent::ComputeSurfaceNormal() const
{
	// Mirrors the liquid material: lean the chosen up vector by the slosh tilt in world XY.
	const FVector Up = GetComponentQuat().GetAxisZ();
	const FVector BaseUp = FMath::Lerp(Up, FVector::UpVector, GetSurfaceUpBlend()).GetSafeNormal();
	return (BaseUp + FVector(Tilt.X, Tilt.Y, 0.0)).GetSafeNormal();
}
