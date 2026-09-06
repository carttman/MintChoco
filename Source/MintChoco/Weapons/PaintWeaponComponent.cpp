#include "Weapons/PaintWeaponComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Paint/PaintLog.h"

UPaintWeaponComponent::UPaintWeaponComponent()
{
	// Only a held Continuous trigger needs a tick, and it turns the tick on for exactly that long.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// The shot RPCs travel on this component, which requires it to replicate.
	SetIsReplicatedByDefault(true);
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

void UPaintWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPaintWeaponComponent, Profile);
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

	// The server fires with its own copy, so a swap made on the owning client has to reach it.
	if (!HasAuthority())
	{
		ServerSetProfile(NewProfile);
	}
}

void UPaintWeaponComponent::ServerSetProfile_Implementation(UPaintWeaponProfile* NewProfile)
{
	SetProfile(NewProfile);
}

void UPaintWeaponComponent::OnRep_Profile()
{
	// The server overruled a profile this owner had already switched to, or swapped it outright;
	// either way a held trigger belongs to the old profile and must not carry on into this one.
	ReleaseTrigger();
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

bool UPaintWeaponComponent::HasAuthority() const
{
	const AActor* const Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

bool UPaintWeaponComponent::FireOnce()
{
	if (!bTriggerHeld || !Profile || !GetWorld())
	{
		return false;
	}

	FVector ViewOrigin;
	FVector ViewDirection;
	GetOwnerView(ViewOrigin, ViewDirection);

	FPaintFireContext Context;
	BuildContext(Context, ViewOrigin, ViewDirection);
	Context.bAuthority = HasAuthority();

	// The seed is spent by the profile's attempt, not by its success; a pinned seed just stays.
	const int32 Seed = NextSeed;
	if (!bUseFixedSeed)
	{
		NextSeed = FMath::Rand();
	}
	Context.Seed = Seed;

	FPaintShot Shot;
	if (!Profile->Fire(Context, Stroke, Shot))
	{
		return false;
	}

	if (Context.bAuthority)
	{
		MulticastShotFired(Shot);
	}
	else
	{
		// The owner sees its ball leave at once and the server's version of the shot never
		// reaches it (the multicast skips the owner), so the two cannot pile up.
		Profile->PlayCosmetic(*Context.World, Context.Instigator, Shot);
		ServerFire(Seed, ViewOrigin, ViewDirection);
	}

	OnFired.Broadcast(Seed);
	return true;
}

void UPaintWeaponComponent::ServerFire_Implementation(int32 Seed, FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!Profile || !GetWorld())
	{
		return;
	}

	FPaintFireContext Context;
	BuildContext(Context, ViewOrigin, ViewDirection);
	Context.Seed = Seed;
	Context.bAuthority = true;

	// The owner already spaced the stroke before asking, and the server never learns when a
	// trigger is released, so a stale anchor here would only swallow the first stamp of the next stroke.
	FPaintStrokeState FreshStroke;
	FPaintShot Shot;
	if (Profile->Fire(Context, FreshStroke, Shot))
	{
		MulticastShotFired(Shot);
		OnFired.Broadcast(Seed);
	}
}

void UPaintWeaponComponent::MulticastShotFired_Implementation(const FPaintShot& Shot)
{
	// The server flies the real ball, the owner its predicted one; both would double up here.
	const APawn* const Pawn = GetOwnerPawn();
	if (HasAuthority() || (Pawn && Pawn->IsLocallyControlled()))
	{
		return;
	}
	if (Profile && GetWorld())
	{
		Profile->PlayCosmetic(*GetWorld(), GetOwnerPawn(), Shot);
	}
}

void UPaintWeaponComponent::BuildContext(FPaintFireContext& OutContext, const FVector& ViewOrigin, const FVector& ViewDirection) const
{
	OutContext.World = GetWorld();
	OutContext.Instigator = GetOwnerPawn();
	OutContext.Muzzle = ComputeMuzzleTransform(ViewOrigin, ViewDirection);
	OutContext.ViewOrigin = ViewOrigin;
	OutContext.ViewDirection = ViewDirection;
	OutContext.PaintId = PaintId;
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
	FVector ViewOrigin;
	FVector ViewDirection;
	GetOwnerView(ViewOrigin, ViewDirection);
	return ComputeMuzzleTransform(ViewOrigin, ViewDirection);
}

FTransform UPaintWeaponComponent::ComputeMuzzleTransform(const FVector& ViewOrigin, const FVector& ViewDirection) const
{
	const AActor* const Owner = GetOwner();
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && !MuzzleSocketName.IsNone() && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketTransform(MuzzleSocketName);
	}

	// Socketless, the muzzle hangs off the view - which on the server is the view the owner sent.
	return FTransform(ViewDirection.Rotation(), ViewOrigin + ViewDirection * MuzzleFallbackOffset);
}
