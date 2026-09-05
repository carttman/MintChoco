#include "Weapons/PaintWeaponComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "Paint/PaintLog.h"

UPaintWeaponComponent::UPaintWeaponComponent()
{
	// Only a held Continuous trigger needs a tick, and it turns the tick on for exactly that long.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPaintWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	NextSeed = FMath::Rand();
	if (Profile)
	{
		Profile->LogUnsetReferences(GetOwner());
	}
}

void UPaintWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseTrigger();
	Super::EndPlay(Reason);
}

void UPaintWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FireOnce();
}

void UPaintWeaponComponent::SetProfile(UPaintWeaponProfile* NewProfile)
{
	if (Profile == NewProfile)
	{
		return;
	}

	ReleaseTrigger();
	Profile = NewProfile;
	if (Profile && HasBegunPlay())
	{
		Profile->LogUnsetReferences(GetOwner());
	}
}

void UPaintWeaponComponent::PullTrigger()
{
	if (bTriggerHeld || !Profile)
	{
		return;
	}

	bTriggerHeld = true;
	Stroke.Reset();
	FireOnce();

	switch (Profile->FireMode)
	{
	case EPaintFireMode::Single:
		break;
	case EPaintFireMode::Automatic:
		GetWorld()->GetTimerManager().SetTimer(
			ShotTimer, this, &UPaintWeaponComponent::OnShotTimer, Profile->GetShotInterval(), /*bLoop=*/true);
		break;
	case EPaintFireMode::Continuous:
		SetComponentTickEnabled(true);
		break;
	}
}

void UPaintWeaponComponent::ReleaseTrigger()
{
	if (!bTriggerHeld)
	{
		return;
	}

	bTriggerHeld = false;
	Stroke.Reset();
	SetComponentTickEnabled(false);
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
}

void UPaintWeaponComponent::SetSeedOverride(bool bInUseFixedSeed, int32 InFixedSeed)
{
	bUseFixedSeed = bInUseFixedSeed;
	// A splat carries 16 bits of seed, so a pinned value is kept in the range a debug box can show.
	NextSeed = bInUseFixedSeed ? FMath::Clamp(InFixedSeed, 0, static_cast<int32>(MAX_uint16)) : FMath::Rand();
}

void UPaintWeaponComponent::OnShotTimer()
{
	FireOnce();
}

bool UPaintWeaponComponent::FireOnce()
{
	FPaintFireContext Context;
	if (!bTriggerHeld || !Profile || !BuildContext(Context))
	{
		return false;
	}

	// The seed is spent by the profile's attempt, not by its success; a pinned seed just stays.
	const int32 Seed = NextSeed;
	if (!bUseFixedSeed)
	{
		NextSeed = FMath::Rand();
	}
	Context.Seed = Seed;

	if (!Profile->Fire(Context, Stroke))
	{
		return false;
	}

	OnFired.Broadcast(Seed);
	return true;
}

bool UPaintWeaponComponent::BuildContext(FPaintFireContext& OutContext) const
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	OutContext.World = World;
	OutContext.Instigator = GetOwnerPawn();
	OutContext.Muzzle = GetMuzzleTransform();
	GetOwnerView(OutContext.ViewOrigin, OutContext.ViewDirection);
	OutContext.PaintId = PaintId;
	return true;
}

APawn* UPaintWeaponComponent::GetOwnerPawn() const
{
	AActor* const Owner = GetOwner();
	if (APawn* const Pawn = Cast<APawn>(Owner))
	{
		return Pawn;
	}
	return Owner ? Owner->GetInstigator() : nullptr;
}

void UPaintWeaponComponent::GetOwnerView(FVector& OutOrigin, FVector& OutDirection) const
{
	// A player aims with the camera, which for a third-person pawn sits nowhere near its eyes;
	// the camera manager is the one place that knows where the player is really looking.
	const APawn* const Pawn = GetOwnerPawn();
	const APlayerController* const PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	FRotator ViewRotation;
	if (PlayerController)
	{
		PlayerController->GetPlayerViewPoint(OutOrigin, ViewRotation);
	}
	else if (Pawn)
	{
		Pawn->GetActorEyesViewPoint(OutOrigin, ViewRotation);
	}
	else
	{
		GetOwner()->GetActorEyesViewPoint(OutOrigin, ViewRotation);
	}
	OutDirection = ViewRotation.Vector();
}

FTransform UPaintWeaponComponent::GetMuzzleTransform() const
{
	const AActor* const Owner = GetOwner();
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && !MuzzleSocketName.IsNone() && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketTransform(MuzzleSocketName);
	}

	FVector ViewOrigin;
	FVector ViewDirection;
	GetOwnerView(ViewOrigin, ViewDirection);
	return FTransform(ViewDirection.Rotation(), ViewOrigin + ViewDirection * MuzzleFallbackOffset);
}
