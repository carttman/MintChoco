#include "Weapons/PaintWeaponComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Ink/InkTankComponent.h"
#include "Items/ItemGameplayTags.h"
#include "Paint/PaintLog.h"

UPaintWeaponComponent::UPaintWeaponComponent()
{
	// Only a held Continuous trigger needs a tick, and it turns the tick on for exactly that long.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// The shot RPCs travel on this component, which requires it to replicate.
	SetIsReplicatedByDefault(true);

	TriggerBlockedTags.AddTag(ItemTags::State_Item_SweetSpinner);
	TriggerBlockedTags.AddTag(ItemTags::State_Item_HeroLanding);
	TriggerBlockedTags.AddTag(ItemTags::State_Status_Stunned);
	FreeShotTags.AddTag(ItemTags::State_Item_InfiniteAmmo);
}

bool UPaintWeaponComponent::IsTriggerBlocked() const
{
	if (TriggerBlockedTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	return AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(TriggerBlockedTags);
}

bool UPaintWeaponComponent::IsShotFree() const
{
	if (FreeShotTags.IsEmpty())
	{
		return false;
	}
	const UAbilitySystemComponent* const AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	return AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(FreeShotTags);
}

void UPaintWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	NextSeed = FMath::Rand();
	Tank = GetOwner() ? GetOwner()->FindComponentByClass<UInkTankComponent>() : nullptr;
	if (Profile)
	{
		Profile->LogUnsetReferences(GetOwner());
	}
}

void UPaintWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// CancelTrigger returns at once when the trigger was already let go, so the loop is stopped
	// here as well: a pawn destroyed mid-charge would otherwise leave it running on the mesh.
	CancelTrigger();
	StopChargeFX();
	Super::EndPlay(Reason);
}

void UPaintWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPaintWeaponComponent, Profile);
	DOREPLIFETIME(UPaintWeaponComponent, PaintId);

	// The owner started its own loop from its own press; sending it back would only restart it late.
	DOREPLIFETIME_CONDITION(UPaintWeaponComponent, bCharging, COND_SkipOwner);
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

	CancelTrigger();
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

void UPaintWeaponComponent::SetPaintId(uint8 NewPaintId)
{
	if (PaintId == NewPaintId) return;

	PaintId = NewPaintId;
	OnPaintIdChanged.Broadcast(PaintId);

	// The server paints with its own copy, so an id picked on the owning client has to reach it.
	// A simulated proxy only mirrors what replication gave it and has no say.
	const APawn* const Pawn = GetOwnerPawn();
	if (!HasAuthority() && Pawn && Pawn->IsLocallyControlled())
	{
		ServerSetPaintId(NewPaintId);
	}
}

void UPaintWeaponComponent::ServerSetPaintId_Implementation(uint8 NewPaintId)
{
	SetPaintId(NewPaintId);
}

void UPaintWeaponComponent::OnRep_PaintId()
{
	OnPaintIdChanged.Broadcast(PaintId);
}

void UPaintWeaponComponent::OnRep_Profile()
{
	// The server overruled a profile this owner had already switched to, or swapped it outright;
	// either way a held trigger belongs to the old profile and must not carry on into this one.
	CancelTrigger();
	if (Profile && HasBegunPlay())
	{
		Profile->LogUnsetReferences(GetOwner());
	}
}

void UPaintWeaponComponent::PullTrigger()
{
	if (bTriggerHeld || !Profile || IsTriggerBlocked())
	{
		return;
	}

	bTriggerHeld = true;
	Stroke.Reset();

	switch (Profile->FireMode)
	{
	case EPaintFireMode::Single:
		FireOnce();
		break;
	case EPaintFireMode::Automatic:
		FireOnce();
		GetWorld()->GetTimerManager().SetTimer(
			ShotTimer, this, &UPaintWeaponComponent::OnShotTimer, Profile->GetShotInterval(), /*bLoop=*/true);
		break;
	case EPaintFireMode::Continuous:
		FireOnce();
		SetComponentTickEnabled(true);
		break;
	case EPaintFireMode::Charged:
		PressTime = GetWorld()->GetTimeSeconds();
		SetCharging(true);
		break;
	}
}

void UPaintWeaponComponent::ReleaseTrigger()
{
	// FireOnce refuses a trigger that is not held, so the charged shot goes before the cancel.
	// GetChargeFraction is 0 outside Charged, so nothing else fires on release.
	const float Charge = GetChargeFraction();
	if (Profile && Profile->FireMode == EPaintFireMode::Charged && Charge > 0.0f && Charge >= Profile->MinChargeToFire)
	{
		PendingChargeFraction = Charge;
		FireOnce();
	}
	CancelTrigger();
}

void UPaintWeaponComponent::CancelTrigger()
{
	if (!bTriggerHeld)
	{
		return;
	}

	bTriggerHeld = false;
	Stroke.Reset();
	// Every way a hold ends - the shot, a cancel, a profile swap - passes through here.
	SetCharging(false);
	SetComponentTickEnabled(false);
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
}

float UPaintWeaponComponent::GetChargeFraction() const
{
	const UWorld* const World = GetWorld();
	if (!bTriggerHeld || !World || !Profile || Profile->FireMode != EPaintFireMode::Charged)
	{
		return 0.0f;
	}
	const double Held = World->GetTimeSeconds() - PressTime;
	return static_cast<float>(FMath::Clamp(Held / FMath::Max(static_cast<double>(Profile->ChargeTime), UE_DOUBLE_KINDA_SMALL_NUMBER), 0.0, 1.0));
}

void UPaintWeaponComponent::SetCharging(bool bNewCharging)
{
	if (HasAuthority())
	{
		bCharging = bNewCharging;
	}
	else
	{
		ServerSetCharging(bNewCharging);
	}

	// The machine that set the value never gets its own OnRep, and a dedicated server draws nothing.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bNewCharging)
		{
			StartChargeFX();
		}
		else
		{
			StopChargeFX();
		}
	}
}

void UPaintWeaponComponent::ServerSetCharging_Implementation(bool bNewCharging)
{
	// The server never saw the press, so it takes the owner's word - but only for a weapon that
	// charges at all, and only while the trigger is allowed. A shot is refused here the same way.
	if (bNewCharging && (!Profile || Profile->FireMode != EPaintFireMode::Charged || IsTriggerBlocked()))
	{
		return;
	}

	bCharging = bNewCharging;

	// A listen server renders this pawn too, and OnRep never fires on the machine that assigned.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bNewCharging)
		{
			StartChargeFX();
		}
		else
		{
			StopChargeFX();
		}
	}
}

void UPaintWeaponComponent::OnRep_Charging()
{
	if (bCharging)
	{
		StartChargeFX();
	}
	else
	{
		StopChargeFX();
	}
}

void UPaintWeaponComponent::StartChargeFX()
{
	if (ChargeFXComponent || !Profile || !Profile->ChargeFX || !GetWorld())
	{
		return;
	}

	const FVector Scale(Profile->ChargeFXScale);
	if (USkeletalMeshComponent* const Mesh = GetMuzzleMesh())
	{
		ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->ChargeFX, Mesh, MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget,
			// Deactivate leaves the last particles to finish and then cleans itself up; false would
			// pile a dead component on the mesh for every charge.
			/*bAutoDestroy=*/true, ENCPoolMethod::None);
		return;
	}

	const FTransform Muzzle = GetMuzzleTransform();
	ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), Profile->ChargeFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale);
}

void UPaintWeaponComponent::StopChargeFX()
{
	if (ChargeFXComponent)
	{
		ChargeFXComponent->Deactivate();
		ChargeFXComponent = nullptr;
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

float UPaintWeaponComponent::GetShotCost() const
{
	// The owner predicts the free shot with its own tag; the server has the tag by the time ServerFire
	// arrives, since the activation RPC travels the same reliable channel ahead of it.
	if (!Profile || IsShotFree()) return 0.0f;

    return Profile->GetInkCostPerShot();
}

bool UPaintWeaponComponent::CanAffordShot() const
{
	return !Tank.IsValid() || Tank->CanAfford(GetShotCost());
}

void UPaintWeaponComponent::SpendShot()
{
	// A free shot does not touch the tank at all, so the refill keeps running through the effect.
	const float Cost = GetShotCost();
	if (Cost > 0.0f && Tank.IsValid())
	{
		Tank->TryConsume(Cost);
	}
}

bool UPaintWeaponComponent::FireOnce()
{
	if (!bTriggerHeld || !Profile || !GetWorld() || !CanAffordShot())
	{
		return false;
	}

	FVector ViewOrigin;
	FVector ViewDirection;
	GetOwnerView(ViewOrigin, ViewDirection);

	// Only a Charged shot carries a partial charge; everything else fires at full strength.
	const float ChargeFraction = Profile->FireMode == EPaintFireMode::Charged ? PendingChargeFraction : 1.0f;
	PendingChargeFraction = 1.0f;

	FPaintFireContext Context;
	BuildContext(Context, ViewOrigin, ViewDirection, ChargeFraction);
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

	// The machines that only replay this shot size their flash by it.
	Shot.Charge = static_cast<uint8>(FMath::RoundToInt(ChargeFraction * 255.0f));

	// With authority this is the real spend; the owner's is a prediction the replicated tank corrects.
	SpendShot();

	if (Context.bAuthority)
	{
		MulticastShotFired(Shot);
	}
	else
	{
		// The owner sees its ball leave at once and the server's version of the shot never
		// reaches it (the multicast skips the owner), so the two cannot pile up.
		Profile->PlayCosmetic(*Context.World, Context.Instigator, Shot);
		ServerFire(Seed, ViewOrigin, ViewDirection, Shot.Charge);
	}

	PlayMuzzleFX(ChargeFraction);
	OnFired.Broadcast(Seed);
	return true;
}

void UPaintWeaponComponent::ServerFire_Implementation(int32 Seed, FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection, uint8 Charge)
{
	// The owner checked its own tank before asking, but only the server's copy is the truth.
	// The same goes for a blocking tag: an owner that fired anyway is refused here.
	if (!Profile || !GetWorld() || !CanAffordShot() || IsTriggerBlocked())
	{
		return;
	}

	FPaintFireContext Context;
	BuildContext(Context, ViewOrigin, ViewDirection, Profile->FireMode == EPaintFireMode::Charged ? Charge / 255.0f : 1.0f);
	Context.Seed = Seed;
	Context.bAuthority = true;

	// The owner already spaced the stroke before asking, and the server never learns when a
	// trigger is released, so a stale anchor here would only swallow the first stamp of the next stroke.
	FPaintStrokeState FreshStroke;
	FPaintShot Shot;
	if (Profile->Fire(Context, FreshStroke, Shot))
	{
		Shot.Charge = Charge;
		SpendShot();
		MulticastShotFired(Shot);
		PlayMuzzleFX(Context.ChargeFraction);
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
	// Feedback on the machines that only watch: the owner and the server raised theirs when they fired.
	PlayMuzzleFX(Shot.Charge / 255.0f);
	OnFired.Broadcast(Shot.Seed);
}

void UPaintWeaponComponent::BuildContext(FPaintFireContext& OutContext, const FVector& ViewOrigin, const FVector& ViewDirection, float ChargeFraction) const
{
	OutContext.World = GetWorld();
	OutContext.Instigator = GetOwnerPawn();
	OutContext.Muzzle = ComputeMuzzleTransform(ViewOrigin, ViewDirection);
	OutContext.ViewOrigin = ViewOrigin;
	OutContext.ViewDirection = ViewDirection;
	OutContext.PaintId = PaintId;
	OutContext.ChargeFraction = FMath::Clamp(ChargeFraction, 0.0f, 1.0f);
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

USkeletalMeshComponent* UPaintWeaponComponent::GetMuzzleMesh() const
{
	AActor* const Owner = GetOwner();
	ACharacter* const Character = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : (Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr);
	return (Mesh && !MuzzleSocketName.IsNone() && Mesh->DoesSocketExist(MuzzleSocketName)) ? Mesh : nullptr;
}

void UPaintWeaponComponent::PlayMuzzleFX(float ChargeFraction)
{
	UWorld* const World = GetWorld();
	if (!Profile || !Profile->MuzzleFX || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector Scale(Profile->GetMuzzleFXScale(ChargeFraction));

	// Attached, so a one-shot flash stays on the barrel while the gun moves.
	if (USkeletalMeshComponent* const Mesh = GetMuzzleMesh())
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->MuzzleFX, Mesh, MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true, ENCPoolMethod::None);
		return;
	}

	// Socketless: the muzzle hangs off the view, so the flash is left where the shot left from.
	const FTransform Muzzle = GetMuzzleTransform();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Profile->MuzzleFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale);
}
