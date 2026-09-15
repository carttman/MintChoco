#include "Weapons/PaintWeaponComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Game/GameGameState.h"
#include "Game/TeamLook.h"
#include "Game/Unit.h"
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
#include "Weapons/PaintAimMath.h"
#include "Weapons/PaintProjectile.h"

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
	// Nobody fires before the match starts (ready wait, countdown). A world without the game
	// state (the sample map) is always allowed.
	if (!AGameGameState::IsPlayerInputAllowed(GetWorld()))
	{
		return true;
	}
	// A unit on the dash board cannot fire. The owner sees its predicted dash, the server the
	// flag from the move, so PullTrigger and ServerFire agree; the dash start also cancels a
	// trigger that was already held (AUnit::HandleDashStateChanged).
	const AUnit* const Unit = Cast<AUnit>(GetOwnerPawn());
	if (Unit && Unit->IsDashing())
	{
		return true;
	}
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

float UPaintWeaponComponent::GetEffectiveShotInterval() const
{
	if (!Profile)
	{
		return 0.0f;
	}
	const float Interval = Profile->GetShotInterval();
	// Continuous has no interval at all; shortening 0 would turn it into a per-tick floor.
	if (Interval <= 0.0f || !IsShotFree())
	{
		return Interval;
	}
	return FMath::Min(Interval, FreeShotInterval);
}

float UPaintWeaponComponent::GetEffectiveChargeTime() const
{
	if (!Profile)
	{
		return 0.0f;
	}
	return IsShotFree() ? FMath::Min(Profile->ChargeTime, FreeShotChargeTime) : Profile->ChargeTime;
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
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeReadyTimer);
	}
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
	const UWorld* const World = GetWorld();
	if (bTriggerHeld || !Profile || !World || IsTriggerBlocked())
	{
		return;
	}

	bTriggerHeld = true;
	Stroke.Reset();

	switch (Profile->FireMode)
	{
	case EPaintFireMode::Single:
		// One pull, one shot, but never faster than the cadence. The trigger still counts as held
		// so the release path stays symmetric; only the shot is skipped.
		if (World->GetTimeSeconds() - LastShotTime >= GetEffectiveShotInterval())
		{
			FireWhenAimReady();
		}
		else
		{
			// 연사 간격에 걸려 이 당김은 넘어가지만, 자세는 유지해야 다음 발이 기다리지 않는다.
			SetAiming(true);
		}
		break;
	case EPaintFireMode::Automatic:
		FireWhenAimReady();
		GetWorld()->GetTimerManager().SetTimer(
			ShotTimer, this, &UPaintWeaponComponent::OnShotTimer, GetEffectiveShotInterval(), /*bLoop=*/true);
		break;
	case EPaintFireMode::Continuous:
		FireWhenAimReady();
		SetComponentTickEnabled(true);
		break;
	case EPaintFireMode::Charged:
		PressTime = GetWorld()->GetTimeSeconds();
		// 충전하는 내내 자세를 든다. 누르는 순간 올라가서 놓을 때까지 그대로다.
		SetAiming(true);
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

	// 준비 중인 첫 발이 있으면 그 발이 나간 뒤에 해제를 마저 한다. 짧게 툭 클릭해도
	// 한 발은 반드시 나가야 하고, 그 발은 자세가 올라온 뒤에 나가야 한다.
	if (bShotPending)
	{
		bCancelAfterPendingShot = true;
		return;
	}

	bTriggerHeld = false;
	Stroke.Reset();
	// Every way a hold ends - the shot, a cancel, a profile swap - passes through here.
	SetCharging(false);
	// 쏜 뒤의 여운은 애님 인스턴스의 FireHoldTime 이 맡는다. 여기서는 “쏘려고 들고 있다” 를 내린다.
	SetAiming(false);
	SetComponentTickEnabled(false);
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
}

void UPaintWeaponComponent::SetAiming(bool bNewAiming)
{
	bAiming = bNewAiming;
}

void UPaintWeaponComponent::FireWhenAimReady()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이미 들고 있거나, 방금 쏴서 자세가 아직 내려오지 않았으면 기다릴 것이 없다.
	const bool bPoseAlreadyUp = bAiming || (World->GetTimeSeconds() - LastShotTime <= AimHoldSeconds);
	SetAiming(true);

	if (bPoseAlreadyUp)
	{
		FireOnce();
		return;
	}

	// 자세가 올라오는 동안 기다린다. 0 이면 다음 틱 — 타이머는 0 이하를 “해제” 로 읽으므로
	// 아주 작은 값을 준다.
	bShotPending = true;
	World->GetTimerManager().SetTimer(AimReadyTimer, this, &UPaintWeaponComponent::FireAfterAimReady,
		FMath::Max(AimReadyDelay, UE_SMALL_NUMBER), /*bLoop=*/false);
}

void UPaintWeaponComponent::FireAfterAimReady()
{
	bShotPending = false;
	FireOnce();

	// 준비를 기다리는 사이에 방아쇠가 풀렸다면 이제 해제를 처리한다.
	if (bCancelAfterPendingShot)
	{
		bCancelAfterPendingShot = false;
		CancelTrigger();
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
	return static_cast<float>(FMath::Clamp(Held / FMath::Max(static_cast<double>(GetEffectiveChargeTime()), UE_DOUBLE_KINDA_SMALL_NUMBER), 0.0, 1.0));
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
		ApplyChargingVisuals(bNewCharging);
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
		ApplyChargingVisuals(bNewCharging);
	}
}

void UPaintWeaponComponent::OnRep_Charging()
{
	ApplyChargingVisuals(bCharging);
}

void UPaintWeaponComponent::ApplyChargingVisuals(bool bNewCharging)
{
	if (bNewCharging)
	{
		StartChargeFX();
		// The ready cue rings when a full charge would be reached. The watchers only know when the
		// hold started, so each machine measures from its own copy of that moment.
		const float ChargeTime = GetEffectiveChargeTime();
		if (UWorld* const World = GetWorld(); World && ChargeTime > 0.0f)
		{
			World->GetTimerManager().SetTimer(ChargeReadyTimer, this, &UPaintWeaponComponent::PlayChargeReadyCue, ChargeTime, /*bLoop=*/false);
		}
	}
	else
	{
		StopChargeFX();
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChargeReadyTimer);
		}
	}

	// 캐릭터가 총을 들고 자세를 잡는 것은 연출만의 일이 아니다: 발사 지점이 그 순간의
	// 총구라서, 자세가 잡혀 있어야 탄이 총열에서 나간다.
	OnChargingChanged.Broadcast(bNewCharging);
}

void UPaintWeaponComponent::StartChargeFX()
{
	if (!Profile || !GetWorld())
	{
		return;
	}

	// The loop rides the muzzle like the FX; without a socket it sits on the owner. Stopped in StopChargeFX.
	if (!ChargeAudioComponent)
	{
		FName AudioSocket = NAME_None;
		USceneComponent* AudioAttachment = GetMuzzleAttachment(AudioSocket);
		if (!AudioAttachment && GetOwner())
		{
			AudioAttachment = GetOwner()->GetRootComponent();
		}
		ChargeAudioComponent = UGameAudioSubsystem::PlayAttached(AudioTags::Audio_Weapon_ChargeLoop, AudioAttachment, AudioSocket, Profile->Sounds);
	}

	if (ChargeFXComponent || !Profile->ChargeFX)
	{
		return;
	}

	const FVector Scale(Profile->ChargeFXScale);
	FName AttachSocket = NAME_None;
	if (USceneComponent* const Attachment = GetMuzzleAttachment(AttachSocket))
	{
		ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->ChargeFX, Attachment, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget,
			// Deactivate leaves the last particles to finish and then cleans itself up; false would
			// pile a dead component on the mesh for every charge.
			/*bAutoDestroy=*/true, ENCPoolMethod::None);
	}
	else
	{
		const FTransform Muzzle = GetMuzzleTransform();
		ChargeFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), Profile->ChargeFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale);
	}

	TintTeamFX(ChargeFXComponent);
}

void UPaintWeaponComponent::StopChargeFX()
{
	if (ChargeFXComponent)
	{
		ChargeFXComponent->Deactivate();
		ChargeFXComponent = nullptr;
	}
	if (ChargeAudioComponent)
	{
		ChargeAudioComponent->Stop();
		ChargeAudioComponent = nullptr;
	}
}

void UPaintWeaponComponent::PlayChargeReadyCue()
{
	if (!Profile)
	{
		return;
	}
	FName Socket = NAME_None;
	USceneComponent* Attachment = GetMuzzleAttachment(Socket);
	if (!Attachment && GetOwner())
	{
		Attachment = GetOwner()->GetRootComponent();
	}
	UGameAudioSubsystem::PlayAttached(AudioTags::Audio_Weapon_ChargeReady, Attachment, Socket, Profile->Sounds);
}

void UPaintWeaponComponent::PlayFireSound()
{
	UWorld* const World = GetWorld();
	if (!Profile || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Attached, like the flash: a shot fired on the move should not leave its sound behind.
	FName Socket = NAME_None;
	USceneComponent* Attachment = GetMuzzleAttachment(Socket);
	if (!Attachment && GetOwner())
	{
		Attachment = GetOwner()->GetRootComponent();
	}
	UGameAudioSubsystem::PlayAttached(AudioTags::Audio_Weapon_Fire, Attachment, Socket, Profile->Sounds);
}

void UPaintWeaponComponent::PlayEmptyCue()
{
	const UWorld* const World = GetWorld();
	const APawn* const Pawn = GetOwnerPawn();
	if (!World || !Profile || !Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastEmptyCueTime < EmptyCueInterval)
	{
		return;
	}
	LastEmptyCueTime = Now;
	UGameAudioSubsystem::Play2D(this, AudioTags::Audio_Weapon_Empty, Profile->Sounds);
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
	if (!bTriggerHeld || !Profile || !GetWorld())
	{
		return false;
	}
	// A dry tank is the one refusal the shooter should hear. The server's copy of this check
	// (ServerFire) stays silent: the owner already heard its own.
	if (!CanAffordShot())
	{
		PlayEmptyCue();
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
		// 발밑 자국은 서버만 찍는다. 스플랫 로그가 결과를 나르므로 클라이언트가 따라 찍을 것이 없다.
		PaintUnderOwner(Context);
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
	PlayFireSound();
	LastShotTime = GetWorld()->GetTimeSeconds();
	OnFired.Broadcast(Seed);
	return true;
}

void UPaintWeaponComponent::PaintUnderOwner(const FPaintFireContext& Context) const
{
	if (!Profile || !Profile->FeetDeposit.CanPaint() || !Context.World || !Context.Instigator)
	{
		return;
	}

	// 폰 중심에서 곧장 아래로. 탄을 쓰지 않으므로 보일 메시도, 풀에서 꺼낼 액터도 없다.
	const FVector Start = Context.Instigator->GetActorLocation();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(PaintWeaponFeet), /*bTraceComplex=*/false, Context.Instigator);

	FHitResult Hit;
	if (!Context.World->LineTraceSingleByChannel(Hit, Start, Start - FVector::UpVector * Profile->FeetTraceDown,
			PaintballChannel, Params)
		|| Hit.bStartPenetrating)
	{
		return;
	}

	// 다른 폰 위에 서 있을 때 그쪽을 때리지 않는다: 발밑 자국은 바닥에만 남는다.
	if (Cast<APawn>(Hit.GetActor()))
	{
		return;
	}

	// 입사 속도를 0으로 넘기면 BuildSplat이 표면 법선을 입사 방향으로 삼아 Stretch가 1이 된다.
	// 발밑 자국은 늘어나지 않고 둥글게 남아야 하므로 이쪽이 맞다.
	Profile->FeetDeposit.ApplyHit(Context.World, Hit, FVector::ZeroVector, Context.PaintId, Context.Seed);
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
		PlayFireSound();
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
	PlayFireSound();
	OnFired.Broadcast(Shot.Seed);
}

void UPaintWeaponComponent::BuildContext(FPaintFireContext& OutContext, const FVector& ViewOrigin, const FVector& ViewDirection, float ChargeFraction) const
{
	OutContext.World = GetWorld();
	OutContext.Instigator = GetOwnerPawn();

	// The physics leaves the sight line at the pawn's depth, the same on the owner and on the
	// server that only got the view; the socket merely says where the shot looks like it left.
	const AActor* const Anchor = OutContext.Instigator ? static_cast<const AActor*>(OutContext.Instigator) : GetOwner();
	const FVector Origin = PaintAim::FireOrigin(ViewOrigin, ViewDirection, Anchor ? Anchor->GetActorLocation() : ViewOrigin, FireOriginForwardMargin);
	OutContext.Muzzle = FTransform(ViewDirection.Rotation(), Origin);
	OutContext.VisualMuzzle = ComputeVisualMuzzle(ViewOrigin, ViewDirection).GetLocation();
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
	return ComputeVisualMuzzle(ViewOrigin, ViewDirection);
}

bool UPaintWeaponComponent::PredictNextImpact(FVector& OutViewOrigin, FVector& OutViewDirection, FVector& OutAimPoint, FVector& OutImpact) const
{
	if (!Profile || !GetWorld())
	{
		return false;
	}
	GetOwnerView(OutViewOrigin, OutViewDirection);

	FPaintFireContext Context;
	BuildContext(Context, OutViewOrigin, OutViewDirection, 1.0f);
	Context.Seed = NextSeed;
	Context.bAuthority = false;
	return Profile->PredictImpact(Context, OutAimPoint, OutImpact);
}


void UPaintWeaponComponent::SetMuzzleSource(USceneComponent* Component, FName SocketName)
{
	MuzzleSource = Component;
	MuzzleSourceSocket = SocketName;
}

FTransform UPaintWeaponComponent::ComputeVisualMuzzle(const FVector& ViewOrigin, const FVector& ViewDirection) const
{
	// A weapon mesh carries its own muzzle. It is checked first so the shot appears to leave the
	// barrel rather than the hand that holds it; the socket lives on the mesh because barrel
	// lengths differ between characters that share one skeleton.
	if (const USceneComponent* const Source = MuzzleSource.Get())
	{
		if (!MuzzleSourceSocket.IsNone() && Source->DoesSocketExist(MuzzleSourceSocket))
		{
			return Source->GetSocketTransform(MuzzleSourceSocket);
		}
	}

	const AActor* const Owner = GetOwner();
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && !VisualMuzzleSocketName.IsNone() && Mesh->DoesSocketExist(VisualMuzzleSocketName))
	{
		return Mesh->GetSocketTransform(VisualMuzzleSocketName);
	}

	// Socketless, the visual muzzle hangs off the view - which on the server is the view the owner sent.
	return FTransform(ViewDirection.Rotation(), ViewOrigin + ViewDirection * VisualMuzzleFallbackOffset);
}

USkeletalMeshComponent* UPaintWeaponComponent::GetMuzzleMesh() const
{
	AActor* const Owner = GetOwner();
	ACharacter* const Character = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* const Mesh =
		Character ? Character->GetMesh() : (Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr);
	return (Mesh && !VisualMuzzleSocketName.IsNone() && Mesh->DoesSocketExist(VisualMuzzleSocketName)) ? Mesh : nullptr;
}

USceneComponent* UPaintWeaponComponent::GetMuzzleAttachment(FName& OutSocket) const
{
	// 총 메시가 총구를 갖고 있으면 그쪽이 먼저다. ComputeVisualMuzzle 과 같은 순서라야
	// 탄이 나오는 것으로 보이는 곳과 불꽃이 피는 곳이 같다.
	if (USceneComponent* const Source = MuzzleSource.Get())
	{
		if (!MuzzleSourceSocket.IsNone() && Source->DoesSocketExist(MuzzleSourceSocket))
		{
			OutSocket = MuzzleSourceSocket;
			return Source;
		}
	}

	if (USkeletalMeshComponent* const Mesh = GetMuzzleMesh())
	{
		OutSocket = VisualMuzzleSocketName;
		return Mesh;
	}

	OutSocket = NAME_None;
	return nullptr;
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
	FName AttachSocket = NAME_None;
	if (USceneComponent* const Attachment = GetMuzzleAttachment(AttachSocket))
	{
		TintTeamFX(UNiagaraFunctionLibrary::SpawnSystemAttached(
			Profile->MuzzleFX, Attachment, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			Scale, EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true, ENCPoolMethod::None));
		return;
	}

	// Socketless: the muzzle hangs off the view, so the flash is left where the shot left from.
	const FTransform Muzzle = GetMuzzleTransform();
	TintTeamFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Profile->MuzzleFX, Muzzle.GetLocation(), Muzzle.Rotator(), Scale));
}

void UPaintWeaponComponent::TintTeamFX(UNiagaraComponent* FX) const
{
	if (FX)
	{
		FX->SetVariableLinearColor(TeamLook::NiagaraTintParameter, TeamLook::GetColor(PaintId, GetWorld()));
	}
}
