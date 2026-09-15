// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Unit.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Game/GameGameState.h"
#include "Game/GamePlayerState.h"
#include "Game/TeamTypes.h"
#include "Game/UnitInputConfig.h"
#include "Game/UnitMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "Ink/InkBottleComponent.h"
#include "Ink/InkTankComponent.h"
#include "InputActionValue.h"
#include "Items/ItemGameplayEffect.h"
#include "Items/ItemGameplayTags.h"
#include "Items/ItemSettings.h"
#include "Items/ItemSlotComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MintChoco.h"
#include "Net/UnrealNetwork.h"
#include "Paint/PaintSplat.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Weapons/PaintWeaponComponent.h"

namespace
{
	/** 총 메시에 있는 총구 소켓. 발사 지점과 총구 화염이 같이 쓴다. */
	const FName GunMuzzleSocketName(TEXT("Muzzle"));
}

AUnit::AUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUnitMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// 유닛 자체가 매 프레임 할 일은 없다. 이동은 CharacterMovement가 돌리고,
	// 발사와 스킬은 각자의 컴포넌트가 필요한 동안만 틱한다.
	PrimaryActorTick.bCanEverTick = false;

	// 몸통 요는 무브먼트 컴포넌트가 돌린다(UUnitMovementComponent::PhysicsRotation).
	//
	// 가만히 서서 둘러볼 때는 카메라만 돌고 몸통은 그대로다. 이동 입력이 있거나 쏘는 동안에만
	// 컨트롤 Yaw를 향해 RotationRate로 돈다. 조준 방향과 정면이 일치해야 총구가 화면 중앙을
	// 향하므로, 쏘는 동안은 몸통이 카메라를 따르고 이동은 카메라 기준 스트레이프다. 여기서
	// 컨트롤 Yaw를 직접 붙이면 매 프레임 스냅되어 둘러보기가 불가능해진다.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.0f;

	// 어깨 너머로 살짝 밀어 화면 중앙을 캐릭터가 가리지 않게 한다.
	CameraBoom->SocketOffset = FVector(0.0f, 60.0f, 60.0f);

	// 붐은 부모(캡슐)의 회전이 아니라 컨트롤 회전을 쓴다. 그래서 캐릭터가 회전해도
	// 카메라가 같이 끌려가지 않는다.
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// 붐이 이미 회전을 처리했으므로 카메라가 다시 하면 이중으로 돈다.
	FollowCamera->bUsePawnControlRotation = false;

	// 카메라 프로브: 다른 유닛의 캡슐(Pawn)만 Overlap. 물리 없음, 로컬 플레이어 폰에서만 켠다.
	CameraProbe = CreateDefaultSubobject<USphereComponent>(TEXT("CameraProbe"));
	CameraProbe->SetupAttachment(FollowCamera);
	CameraProbe->InitSphereRadius(40.0f);
	CameraProbe->SetCollisionObjectType(ECC_WorldDynamic);
	CameraProbe->SetCollisionResponseToAllChannels(ECR_Ignore);
	CameraProbe->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CameraProbe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CameraProbe->SetGenerateOverlapEvents(true);
	CameraProbe->SetCanEverAffectNavigation(false);
	CameraProbe->SetHiddenInGame(true);

	// 다른 플레이어의 카메라 붐이 이 유닛에 걸리지 않는다. 겹침은 위 프로브가 알린다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	PaintWeapon = CreateDefaultSubobject<UPaintWeaponComponent>(TEXT("PaintWeapon"));
	SecondaryWeapon = CreateDefaultSubobject<UPaintWeaponComponent>(TEXT("SecondaryWeapon"));
	InkTank = CreateDefaultSubobject<UInkTankComponent>(TEXT("InkTank"));

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	ItemSlot = CreateDefaultSubobject<UItemSlotComponent>(TEXT("ItemSlot"));

	// 병은 스켈레탈 메시의 InkBottle 소켓에 붙는다. 소켓은 메시가 UnitData로 정해진 뒤에야
	// 존재하므로 여기서는 메시에만 붙이고, ApplyUnitData가 소켓으로 옮긴다. 소켓 위치는
	// 메시 에셋마다 정하므로 캐릭터가 바뀌어도 코드는 그대로다.
	InkBottle = CreateDefaultSubobject<UInkBottleComponent>(TEXT("InkBottle"));
	InkBottle->SetupAttachment(GetMesh());

	InkGlass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InkGlass"));
	InkGlass->SetupAttachment(InkBottle);
	InkGlass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InkGlass->SetGenerateOverlapEvents(false);

	InkSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InkSurface"));
	InkSurface->SetupAttachment(InkBottle);
	InkSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InkSurface->SetGenerateOverlapEvents(false);
	InkBottle->SetSurfaceMesh(InkSurface);

	// 총도 메시의 소켓(Gun)에 붙는다. 소켓은 UnitData가 메시를 정한 뒤에야 존재하므로
	// 여기서는 메시에만 붙이고 ApplyUnitData가 소켓으로 옮긴다(잉크병과 같은 이유).
	// 평소에는 숨어 있고 발사 연출이 켜 준다.
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GetMesh());
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetGenerateOverlapEvents(false);
	GunMesh->SetCanEverAffectNavigation(false);
	GunMesh->SetVisibility(false);

	// 보드도 같은 규칙. Board 소켓은 ApplyUnitData가 메시를 정한 뒤 붙인다.
	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(GetMesh());
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoardMesh->SetGenerateOverlapEvents(false);
	BoardMesh->SetCanEverAffectNavigation(false);
	BoardMesh->SetVisibility(false);

	// 테두리 껍데기. 캐릭터 메시와 같은 메시를 쓰고 포즈는 리더 포즈로 따라가므로 애니메이션을
	// 두 번 돌리지 않는다. 그림자는 원본이 이미 드리우므로 끈다.
	OutlineMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OutlineMesh"));
	OutlineMesh->SetupAttachment(GetMesh());
	OutlineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OutlineMesh->SetGenerateOverlapEvents(false);
	OutlineMesh->SetCanEverAffectNavigation(false);
	OutlineMesh->SetCastShadow(false);
	OutlineMesh->SetVisibility(false);
}

void AUnit::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 어빌리티를 주기 전에 액터 정보가 서 있어야 한다. 아이템 습득은 이보다 뒤다.
	InitAbilityActorInfo();
	ApplyTeamToWeapon();
}

void AUnit::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
	ApplyTeamToWeapon();
}

UAbilitySystemComponent* AUnit::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AUnit::InitAbilityActorInfo()
{
	if (AbilitySystem)
	{
		// 소유자도 아바타도 이 폰이다. Mixed 모드가 요구하는 "소유자의 Owner가 컨트롤러"는
		// 빙의된 폰이 자연히 만족한다.
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

bool AUnit::IsSpeedBoostAuthorized() const
{
	return ItemSlot && ItemSlot->IsSpeedBoostAuthorized();
}

void AUnit::BeginPlay()
{
	Super::BeginPlay();

	// 스턴 태그는 모든 머신에 복제되므로 여기서 받으면 연출과 방아쇠 해제가 어디서나 맞는다.
	if (AbilitySystem)
	{
		StunTagHandle = AbilitySystem->RegisterGameplayTagEvent(ItemTags::State_Status_Stunned, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AUnit::HandleStunTagChanged);

		SuperArmorTagHandle = AbilitySystem->RegisterGameplayTagEvent(ItemTags::State_Status_SuperArmor, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AUnit::HandleSuperArmorTagChanged);
	}

	// 빙의가 BeginPlay보다 먼저 온 경우(리슨 호스트)를 위해 한 번 더 맞춘다.
	UpdateCameraProbe();
}

void AUnit::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	UpdateCameraProbe();
}

void AUnit::PawnClientRestart()
{
	Super::PawnClientRestart();
	// 클라이언트에서는 Controller 복제 순서에 따라 NotifyControllerChanged가 로컬 판정 전에 올 수 있다.
	// ClientRestart는 컨트롤러가 붙은 뒤 소유 머신에서만 오므로 여기서 확실히 켠다.
	UpdateCameraProbe();
}

void AUnit::UnPossessed()
{
	Super::UnPossessed();
	UpdateCameraProbe();
}

void AUnit::UpdateCameraProbe()
{
	if (!CameraProbe)
	{
		return;
	}

	const bool bWantsProbe = IsLocallyControlled() && IsPlayerControlled() && GetNetMode() != NM_DedicatedServer;
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 카메라 프로브 %s (로컬 %d, 플레이어 %d)"),
		*GetNameSafe(this), bWantsProbe ? TEXT("켬") : TEXT("끔"), IsLocallyControlled(), IsPlayerControlled());
	if (bWantsProbe)
	{
		CameraProbe->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}

	CameraProbe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (const TWeakObjectPtr<AUnit>& Faded : CameraFadedUnits)
	{
		if (AUnit* const Other = Faded.Get())
		{
			Other->SetCameraFaded(false);
		}
	}
	CameraFadedUnits.Reset();
}

void AUnit::OnCameraProbeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUnit* const Other = Cast<AUnit>(OtherActor);
	if (!Other || Other == this || OtherComp != Other->GetCapsuleComponent())
	{
		return;
	}
	CameraFadedUnits.AddUnique(Other);
	Other->SetCameraFaded(true);
}

void AUnit::OnCameraProbeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AUnit* const Other = Cast<AUnit>(OtherActor);
	if (!Other || Other == this || OtherComp != Other->GetCapsuleComponent())
	{
		return;
	}
	CameraFadedUnits.Remove(Other);
	Other->SetCameraFaded(false);
}

void AUnit::SetCameraFaded(bool bFaded)
{
	USkeletalMeshComponent* const MeshComponent = GetMesh();
	if (!MeshComponent || bFaded == bCameraFaded)
	{
		return;
	}

	if (bFaded)
	{
		if (!CameraFadeMaterial)
		{
			return;
		}
		CameraFadeOriginalMaterials.Reset();
		for (UMaterialInterface* const Original : MeshComponent->GetMaterials())
		{
			CameraFadeOriginalMaterials.Add(Original);
		}
		for (int32 Index = 0; Index < CameraFadeOriginalMaterials.Num(); ++Index)
		{
			MeshComponent->SetMaterial(Index, CameraFadeMaterial);
		}
		bCameraFaded = true;
		UpdateGunVisibility();
		UpdateBoardVisibility();
		UpdateSuperArmorOutline();
		return;
	}

	for (int32 Index = 0; Index < CameraFadeOriginalMaterials.Num(); ++Index)
	{
		MeshComponent->SetMaterial(Index, CameraFadeOriginalMaterials[Index]);
	}
	CameraFadeOriginalMaterials.Reset();
	bCameraFaded = false;
	UpdateGunVisibility();
	UpdateBoardVisibility();
	UpdateSuperArmorOutline();
}

int32 AUnit::GetTeam() const
{
	const AGamePlayerState* const GamePlayerState = GetPlayerState<AGamePlayerState>();
	return GamePlayerState ? GamePlayerState->GetTeam() : Teams::None;
}

bool AUnit::IsStunned() const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Status_Stunned);
}

bool AUnit::HasSuperArmor() const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Status_SuperArmor);
}

bool AUnit::WantsToFaceAim() const
{
	auto Weapons = { PaintWeapon.Get(), SecondaryWeapon.Get() };

	for (const auto Weapon : Weapons)
	{
		if (!Weapon) continue;

		if (Weapon->IsTriggerHeld() || Weapon->IsAiming() || Weapon->IsCharging())
		{
			return true;
		}
	}
	const UWorld* const World = GetWorld();
	return World && LastFireTime >= 0.0 && World->GetTimeSeconds() - LastFireTime <= FaceAimHoldSeconds;
}

bool AUnit::IsMovementInputLocked() const
{
	// 경기 전(전원 대기, 카운트다운)에는 아무도 움직이지 못한다. 단계는 복제되므로 양쪽이 같은 답을 본다.
	if (!AGameGameState::IsPlayerInputAllowed(GetWorld()))
	{
		return true;
	}
	if (!AbilitySystem)
	{
		return false;
	}
	return AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Status_Stunned)
		|| AbilitySystem->HasMatchingGameplayTag(ItemTags::State_Item_HeroLanding);
}

bool AUnit::CanJumpInternal_Implementation() const
{
	return !IsMovementInputLocked() && Super::CanJumpInternal_Implementation();
}

void AUnit::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 착지는 낙하를 계산하는 머신(소유자와 서버)에만 온다. 다른 플레이어의 착지음은 그래서 없다.
	UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_Unit_Land, Hit.ImpactPoint, UnitData ? UnitData->Sounds.Get() : nullptr);

	// 히어로 랜딩의 내리꽂기가 끝났다. 단계 정리는 무브먼트 컴포넌트가, 효과는 어빌리티가 맡는다.
	if (UUnitMovementComponent* const Movement = GetUnitMovement())
	{
		if (Movement->FinishHeroLandingDive())
		{
			OnHeroLandingFinished.Broadcast();
		}
	}
}

bool AUnit::ApplyStatusEffect(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& StatusTag, float Duration, FActiveGameplayEffectHandle& OutHandle)
{
	if (!AbilitySystem || !EffectClass || Duration <= 0.0f)
	{
		return false;
	}

	// 아이템 GE와 같은 골격: 지속시간은 SetByCaller, 태그는 스펙의 동적 태그.
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(ItemTags::Data_Item_Duration, Duration);
	Spec.Data->DynamicGrantedTags.AddTag(StatusTag);
	OutHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	return OutHandle.WasSuccessfullyApplied();
}

bool AUnit::TryApplyStun()
{
	const UItemSettings& Settings = UItemSettings::Get();
	return TryApplyStun(Settings.StunDuration, Settings.SuperArmorDuration);
}

bool AUnit::TryApplyStun(float StunSeconds, float SuperArmorSeconds)
{
	if (!HasAuthority() || StunSeconds <= 0.0f || IsStunned() || HasSuperArmor())
	{
		return false;
	}

	FActiveGameplayEffectHandle Handle;
	if (!ApplyStatusEffect(UGE_Stunned::StaticClass(), ItemTags::State_Status_Stunned, StunSeconds, Handle))
	{
		return false;
	}

	// 슈퍼아머는 스턴이 끝나는 바로 그 순간 이어져야 "스턴 2초 후 4초"가 된다. 길이는 건 쪽이 정한다.
	PendingSuperArmorSeconds = SuperArmorSeconds;
	if (FOnActiveGameplayEffectRemoved_Info* const Removed = AbilitySystem->OnGameplayEffectRemoved_InfoDelegate(Handle))
	{
		Removed->AddUObject(this, &AUnit::HandleStunEnded);
	}

	UE_LOG(LogMintChoco, Verbose, TEXT("%s: 스턴 %.2f초, 이어서 슈퍼아머 %.2f초."), *GetNameSafe(this), StunSeconds, SuperArmorSeconds);
	return true;
}

uint8 AUnit::GetPaintId() const
{
	const int32 Team = GetTeam();
	if (Teams::IsValidId(Team))
	{
		return static_cast<uint8>(Team);
	}
	return PaintWeapon ? PaintWeapon->GetPaintId() : PaintIdNone;
}

void AUnit::HandleStunEnded(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	// 제거 알림은 클라이언트에도 복제로 오지만, 거는 것은 서버의 일이다.
	if (!HasAuthority())
	{
		return;
	}
	if (PendingSuperArmorSeconds <= 0.0f)
	{
		return;
	}
	FActiveGameplayEffectHandle Handle;
	ApplyStatusEffect(UGE_SuperArmor::StaticClass(), ItemTags::State_Status_SuperArmor, PendingSuperArmorSeconds, Handle);
}

void AUnit::HandleStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bStunned = NewCount > 0;
	if (bStunned)
	{
		// 누르고 있던 방아쇠는 놓는다. 차지 중이었다면 발사되지 않는다(부분 충전 발사도 없다).
		if (PaintWeapon)
		{
			PaintWeapon->CancelTrigger();
		}
		if (SecondaryWeapon)
		{
			SecondaryWeapon->CancelTrigger();
		}
	}
	// 태그는 모든 머신에 복제되므로 소리도 각자 낸다. 데디케이티드 서버는 서브시스템이 스스로 거른다.
	UGameAudioSubsystem::PlayAttached(
		bStunned ? AudioTags::Audio_Unit_Stun_Begin : AudioTags::Audio_Unit_Stun_End,
		GetRootComponent(), NAME_None, UnitData ? UnitData->Sounds.Get() : nullptr);
	UpdateStunFX(bStunned);
	BP_OnStunned(bStunned);
}

void AUnit::UpdateStunFX(bool bStunned)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bStunned)
	{
		if (StunFXComponent)
		{
			// 대시 트레일과 같다: 이미 태어난 파티클은 수명대로 사라지도록 새 스폰만 멈춘다.
			StunFXComponent->Deactivate();
			StunFXComponent = nullptr;
		}
		return;
	}

	// 스턴은 겹쳐 걸리지 않지만(TryApplyStun이 이미 스턴 중이면 거절한다) 태그 이벤트가 두 번
	// 오더라도 FX가 둘로 늘어나지 않도록 막아 둔다.
	if (StunFXComponent)
	{
		return;
	}

	const FUnitActionFeedback* const Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Stun) : nullptr;
	if (!Feedback || !Feedback->FX)
	{
		return;
	}

	// 소켓 기준 오프셋을 그대로 살려야 머리 위로 띄울 수 있으므로 KeepRelativeOffset이다.
	StunFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Feedback->FX,
		GetMesh(),
		Feedback->FXSocket,
		Feedback->FXOffset,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		// Deactivate 뒤 남은 파티클이 사라지면 스스로 정리된다. false면 스턴마다 꺼진
		// 컴포넌트가 메시에 하나씩 쌓인다.
		true);
}

void AUnit::HandleSuperArmorTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateSuperArmorOutline();
	UGameAudioSubsystem::PlayAttached(
		NewCount > 0 ? AudioTags::Audio_Unit_SuperArmor_Begin : AudioTags::Audio_Unit_SuperArmor_End,
		GetRootComponent(), NAME_None, UnitData ? UnitData->Sounds.Get() : nullptr);
}

void AUnit::UpdateSuperArmorOutline()
{
	if (OutlineMesh)
	{
		// 카메라가 안에 들어와 몸이 반투명해진 동안에는 테두리도 감춘다. 껍데기는 불투명이라
		// 그대로 두면 페이드된 몸 위에 실루엣만 둥둥 뜬다.
		OutlineMesh->SetVisibility(HasSuperArmor() && !bCameraFaded);
	}
}

void AUnit::Knockback(const FVector& From)
{
	if (!HasAuthority() || HasSuperArmor())
	{
		return;
	}

	const UItemSettings& Settings = UItemSettings::Get();
	FVector Direction = GetActorLocation() - From;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		// 정확히 위에서 터졌다. 뒤로 민다.
		Direction = -GetActorForwardVector();
	}

	// 서버만 건다. 소유 클라이언트에는 다음 보정이 새 속도를 실어 나른다. 클라이언트 RPC로 같이
	// 걸면 보정과 순서가 어긋나 오히려 보정이 늘어난다.
	LaunchCharacter(Direction * Settings.KnockbackSpeed + FVector(0.0f, 0.0f, Settings.KnockbackUpSpeed), /*bXYOverride=*/true, /*bZOverride=*/true);
}

void AUnit::ApplyTeamToWeapon()
{
	// 팀 번호가 곧 페인트 id다(민트 0, 초코 1). 팀이 없는 PlayerState(샘플 맵)는
	// 건드리지 않아, 다른 곳에서 정해 준 id가 남는다.
	const AGamePlayerState* GamePlayerState = GetPlayerState<AGamePlayerState>();
	if (!GamePlayerState || !Teams::IsValidId(GamePlayerState->GetTeam()))
	{
		return;
	}

	// 병 색은 무기의 페인트 id를 따라가므로(HandlePaintIdChanged) 여기서 따로 칠하지 않는다.
	const uint8 PaintId = static_cast<uint8>(GamePlayerState->GetTeam());
	if (PaintWeapon)
	{
		PaintWeapon->SetPaintId(PaintId);
	}
	if (SecondaryWeapon)
	{
		SecondaryWeapon->SetPaintId(PaintId);
	}
}

void AUnit::HandlePaintIdChanged(uint8 PaintId)
{
	if (InkBottle)
	{
		InkBottle->SetTeam(PaintId);
	}
}

void AUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BeginPlay보다 이른 시점이라 첫 프레임이 그려지기 전에 메시가 확정된다.
	// 여기서 보이는 값은 클라이언트에서도 블루프린트 기본값이므로, 런타임에
	// 교체된 경우는 OnRep_UnitData가 뒤이어 처리한다.
	ApplyUnitData();

	// 병은 무기의 페인트 id 하나만 본다. 팀(PlayerState)이든 샘플 맵의 휠이든 어디서 정해도
	// 그 값은 무기에서 복제되므로, 다른 클라이언트의 병도 같은 경로로 색이 맞는다.
	if (PaintWeapon)
	{
		PaintWeapon->OnPaintIdChanged.AddDynamic(this, &AUnit::HandlePaintIdChanged);
		HandlePaintIdChanged(PaintWeapon->GetPaintId());
		PaintWeapon->OnFired.AddDynamic(this, &AUnit::HandleWeaponFired);
	}
	if (SecondaryWeapon)
	{
		SecondaryWeapon->OnFired.AddDynamic(this, &AUnit::HandleWeaponFired);
	}

	// 블루프린트가 캡슐·메시 충돌을 덮어썼어도 카메라 채널만은 여기서 다시 무시로 둔다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (CameraProbe)
	{
		CameraProbe->OnComponentBeginOverlap.AddDynamic(this, &AUnit::OnCameraProbeBeginOverlap);
		CameraProbe->OnComponentEndOverlap.AddDynamic(this, &AUnit::OnCameraProbeEndOverlap);
	}

	// 벽 옆면 스플랫은 데칼이라 박스 안에 들어온 유닛에도 묻는다. 블루프린트가 붙인 메시까지 전부 받지 않게 한다.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		Primitive->SetReceivesDecals(false);
	}

	// 소유 클라이언트에서는 입력이, 서버에서는 압축 플래그가 이 알림을 낸다.
	// 어느 쪽이든 실제로 상태가 바뀔 때만 한 번씩 온다.
	if (UUnitMovementComponent* Movement = GetUnitMovement())
	{
		Movement->OnDashStateChanged.AddUObject(this, &AUnit::HandleDashStateChanged);
		if (PaintWeapon)
		{
			PaintWeapon->OnChargingChanged.AddDynamic(this, &AUnit::HandleChargingChanged);
		}
		if (SecondaryWeapon)
		{
			SecondaryWeapon->OnChargingChanged.AddDynamic(this, &AUnit::HandleChargingChanged);
		}
		Movement->OnHeroLandingPhaseChanged.AddUObject(this, &AUnit::HandleHeroLandingPhaseChanged);
	}
	else
	{
		UE_LOG(LogMintChoco, Error,
			TEXT("%s: 무브먼트 컴포넌트가 UUnitMovementComponent가 아닙니다(현재 %s). 대시가 동작하지 않습니다. ")
			TEXT("블루프린트를 열어 컴파일 후 저장하면 상속 컴포넌트가 갱신됩니다."),
			*GetNameSafe(this), *GetNameSafe(GetCharacterMovement()->GetClass()));
	}
}

UUnitMovementComponent* AUnit::GetUnitMovement() const
{
	return Cast<UUnitMovementComponent>(GetCharacterMovement());
}

void AUnit::ApplyViewPitchLimits()
{
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (APlayerCameraManager* const CameraManager =
			PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr)
	{
		CameraManager->ViewPitchMin = ViewPitchMin;
		CameraManager->ViewPitchMax = ViewPitchMax;
	}
}

void AUnit::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// InputConfig가 비어 조작을 못 묶더라도 시야 제한은 걸어 둔다. 아래 이른 반환보다 앞에 둔 이유다.
	ApplyViewPitchLimits();

	if (!InputConfig)
	{
		UE_LOG(LogMintChoco, Error, TEXT("%s: InputConfig가 비어 있어 조작을 받을 수 없습니다."),
			*GetNameSafe(this));
		return;
	}

	// 이 함수는 로컬 플레이어가 조종하는 폰에서만 호출된다. AI가 빙의한 폰에는 오지
	// 않으므로, 여기서 컨텍스트를 넣으면 별도의 가드 없이 대상이 정확히 걸러진다.
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;

	if (InputConfig->GameplayContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->AddMappingContext(InputConfig->GameplayContext, InputConfig->GameplayContextPriority);
			AppliedInputSubsystem = Subsystem;
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogMintChoco, Error, TEXT("%s: EnhancedInputComponent가 아닙니다. 프로젝트 설정의 입력 클래스를 확인하세요."),
			*GetNameSafe(this));
		return;
	}

	if (InputConfig->MoveAction)
	{
		EnhancedInput->BindAction(InputConfig->MoveAction, ETriggerEvent::Triggered, this, &AUnit::Move);
	}

	if (InputConfig->LookAction)
	{
		EnhancedInput->BindAction(InputConfig->LookAction, ETriggerEvent::Triggered, this, &AUnit::Look);
	}

	if (InputConfig->DashAction)
	{
		EnhancedInput->BindAction(InputConfig->DashAction, ETriggerEvent::Started, this, &AUnit::StartDash);
		EnhancedInput->BindAction(InputConfig->DashAction, ETriggerEvent::Completed, this, &AUnit::StopDash);

		// 키를 누른 채 매핑 컨텍스트가 재구성되거나 제거되면 Completed 대신 Canceled가 온다.
		// 이걸 빼면 그 상황에서 대시가 켜진 채로 남는다.
		EnhancedInput->BindAction(InputConfig->DashAction, ETriggerEvent::Canceled, this, &AUnit::StopDash);
	}

	if (InputConfig->JumpAction)
	{
		EnhancedInput->BindAction(InputConfig->JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);

		// ACharacter::Jump는 bPressedJump를 세울 뿐이라 StopJumping이 없으면
		// 계속 눌린 상태로 남아 착지할 때마다 다시 점프한다.
		EnhancedInput->BindAction(InputConfig->JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	// 연사와 붓은 누르고 있는 동안 계속 나가야 하므로 놓는 쪽도 묶는다.
	// Canceled는 다른 입력이 가로채거나 누른 채로 매핑이 빠질 때(EndPlay)이며, 그때는 방아쇠를
	// 놓되 쏘지는 않는다. 차지형 무기는 놓는 순간 발사되므로 둘을 구분해야 한다.
	if (InputConfig->FireAction)
	{
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Started, this, &AUnit::StartFire);
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Completed, this, &AUnit::StopFire);
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Canceled, this, &AUnit::CancelFire);
	}

	if (InputConfig->SecondaryFireAction)
	{
		EnhancedInput->BindAction(InputConfig->SecondaryFireAction, ETriggerEvent::Started, this, &AUnit::StartSecondaryFire);
		EnhancedInput->BindAction(InputConfig->SecondaryFireAction, ETriggerEvent::Completed, this, &AUnit::StopSecondaryFire);
		EnhancedInput->BindAction(InputConfig->SecondaryFireAction, ETriggerEvent::Canceled, this, &AUnit::CancelSecondaryFire);
	}

	if (InputConfig->ItemAction)
	{
		EnhancedInput->BindAction(InputConfig->ItemAction, ETriggerEvent::Started, this, &AUnit::UseItem);
	}

#if !UE_BUILD_SHIPPING
	// 디버그: 숫자 키 1~8이 설정 목록의 아이템을 바로 슬롯에 넣는다. 입력 액션 에셋 없이 키를 직접
	// 묶는다. Enhanced Input이 켜져 있어도 옛 키 바인딩은 그대로 동작한다.
	const FKey DebugItemKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DebugItemKeys); ++Index)
	{
		FInputKeyBinding Binding(FInputChord(DebugItemKeys[Index]), IE_Pressed);
		Binding.bConsumeInput = false;
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Index]()
		{
			if (ItemSlot)
			{
				ItemSlot->DebugGiveItem(Index);
			}
		});
		PlayerInputComponent->KeyBindings.Add(MoveTemp(Binding));
	}
#endif
}

void AUnit::UseItem()
{
	if (ItemSlot)
	{
		ItemSlot->TryUseHeldItem();
	}
}

// 한쪽 방아쇠가 당겨진 동안 다른 쪽 입력은 무시한다. 무시된 눌림의 뗌은 당겨지지 않은
// 컴포넌트에 Release/Cancel로 오는데, 그쪽은 아무것도 하지 않으므로 따로 걸러내지 않는다.
void AUnit::StartFire()
{
	// 효과 중인 아이템이 좌클릭을 먼저 가져간다. 꿀풍선은 조준을 확정해 던지고, 히어로 랜딩은
	// 공중에 멈춰 있으면 그 자리에서 내리꽂는다. 가져갔으면 무기에는 닿지 않는다.
	if (ItemSlot && ItemSlot->HandleFireInput())
	{
		return;
	}

	if (PaintWeapon && !(SecondaryWeapon && SecondaryWeapon->IsTriggerHeld()))
	{
		PaintWeapon->PullTrigger();
	}
}

void AUnit::StopFire()
{
	if (PaintWeapon)
	{
		PaintWeapon->ReleaseTrigger();
	}
}

void AUnit::CancelFire()
{
	if (PaintWeapon)
	{
		PaintWeapon->CancelTrigger();
	}
}

void AUnit::StartSecondaryFire()
{
	// 우클릭은 조준 취소가 먼저다.
	if (ItemSlot && ItemSlot->HandleCancelInput())
	{
		return;
	}

	if (SecondaryWeapon && !(PaintWeapon && PaintWeapon->IsTriggerHeld()))
	{
		SecondaryWeapon->PullTrigger();
	}
}

void AUnit::StopSecondaryFire()
{
	if (SecondaryWeapon)
	{
		SecondaryWeapon->ReleaseTrigger();
	}
}

void AUnit::CancelSecondaryFire()
{
	if (SecondaryWeapon)
	{
		SecondaryWeapon->CancelTrigger();
	}
}

void AUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 죽은 폰의 매핑이 남아 있지 않도록 넣었던 컨텍스트를 되돌린다.
	// 바인딩 자체는 InputComponent와 함께 폰에 딸려 사라지므로 따로 풀 필요가 없다.
	if (InputConfig && InputConfig->GameplayContext && AppliedInputSubsystem.IsValid())
	{
		AppliedInputSubsystem->RemoveMappingContext(InputConfig->GameplayContext);
	}

	AppliedInputSubsystem.Reset();

	// 내 카메라가 반투명하게 만든 유닛을 되돌린다. 파괴 중에는 EndOverlap 알림이 오지 않는다.
	for (const TWeakObjectPtr<AUnit>& Faded : CameraFadedUnits)
	{
		if (AUnit* const Other = Faded.Get())
		{
			Other->SetCameraFaded(false);
		}
	}
	CameraFadedUnits.Reset();

	if (AbilitySystem && StunTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(ItemTags::State_Status_Stunned, EGameplayTagEventType::NewOrRemoved).Remove(StunTagHandle);
	}
	StunTagHandle.Reset();

	if (AbilitySystem && SuperArmorTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(ItemTags::State_Status_SuperArmor, EGameplayTagEventType::NewOrRemoved).Remove(SuperArmorTagHandle);
	}
	SuperArmorTagHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AUnit::Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();

	// 스턴이나 히어로 랜딩 중에는 입력을 버린다. 서버 쪽은 무브먼트 컴포넌트가 같은 규칙으로 막는다.
	if (MoveInput.IsNearlyZero() || IsMovementInputLocked())
	{
		return;
	}

	// 캐릭터의 현재 회전이 아니라 컨트롤 회전을 기준으로 삼는다. 회전이 컨트롤 회전을
	// 따라오는 데는 한 프레임이 걸리므로, 캐릭터 회전을 쓰면 빠르게 시점을 돌릴 때
	// 이동 방향이 미세하게 밀린다.
	const FRotationMatrix YawMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f));

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), MoveInput.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), MoveInput.X);
}

void AUnit::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	// 마우스 델타는 이미 프레임당 이동량이므로 DeltaTime을 곱하면 안 된다. 곱하면
	// 프레임률에 따라 감도가 달라진다. 게임패드 스틱을 붙일 때는 반대로 곱해야 하며,
	// 그래서 스틱은 별도의 InputAction으로 분리하게 된다.
	//
	// 감도와 Y축 반전은 InputAction의 Modifier가 이미 처리한 뒤다.
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AUnit::StartDash()
{
	SetDashInput(true);
}

void AUnit::StopDash()
{
	SetDashInput(false);
}

void AUnit::SetDashInput(bool bWantsToDash)
{
	UUnitMovementComponent* Movement = GetUnitMovement();
	if (!Movement)
	{
		return;
	}

	// 속도를 여기서 건드리지 않는다. 의도만 세우면 압축 플래그를 통해 서버까지 가고,
	// 실제 속도는 양쪽의 GetMaxSpeed()가 같은 규칙으로 계산한다.
	Movement->SetWantsToDash(bWantsToDash);
}

void AUnit::HandleDashStateChanged(bool bDashing)
{
	// 이 알림은 무브먼트 플래그가 실제로 바뀐 머신(소유 클라이언트와 서버)에서만 온다. 소유
	// 클라이언트는 복제에서 제외되어 있으므로(COND_SkipOwner) 여기서 직접 써야 자기 화면의
	// IsDashing()이 예측값을 본다. 서버가 쓴 값은 나머지 클라이언트에게만 복제된다.
	bIsDashing = bDashing;

	// 보드를 타는 동안은 쏘지 못한다. 누르고 있던 방아쇠는 놓고, 충전 중이던 차지샷은 발사 없이
	// 취소된다. 서버도 이 알림을 받으므로 ServerFire의 IsTriggerBlocked와 어긋나지 않는다.
	if (bDashing)
	{
		if (PaintWeapon)
		{
			PaintWeapon->CancelTrigger();
		}
		if (SecondaryWeapon)
		{
			SecondaryWeapon->CancelTrigger();
		}
	}

	UpdateDashEffects(bDashing);
}

void AUnit::HandleHeroLandingPhaseChanged(EHeroLandingPhase NewPhase)
{
	// 복제는 서버만 한다. 소유 클라이언트도 이 알림을 받지만 자기 값은 무브먼트에서 직접 읽는다.
	if (HasAuthority())
	{
		ReplicatedHeroPhase = NewPhase;
	}
}

void AUnit::OnRep_HeroLandingPhase()
{
	// 프록시의 무브먼트는 단계 기계를 돌리지 않아 단계를 모른다. 내리꽂기가 중력 없는 직선인
	// 것도 단계로 판단하므로(GetGravityZ), 복제된 값을 넣어 주지 않으면 프록시만 중력을 더
	// 받아 서버보다 빨리 가라앉는다.
	if (UUnitMovementComponent* const Movement = GetUnitMovement())
	{
		Movement->SetSimulatedHeroLandingPhase(ReplicatedHeroPhase);
	}
}

EHeroLandingPhase AUnit::GetHeroLandingPhase() const
{
	// 이 머신이 단계 기계를 직접 돌리는 경우(소유자, 서버)에는 그 값이 가장 빠르고 정확하다.
	if (IsLocallyControlled() || HasAuthority())
	{
		const UUnitMovementComponent* const Movement = GetUnitMovement();
		return Movement ? Movement->GetHeroLandingPhase() : EHeroLandingPhase::None;
	}
	return ReplicatedHeroPhase;
}

void AUnit::OnRep_IsDashing()
{
	UpdateDashEffects(bIsDashing);
}

void AUnit::UpdateDashEffects(bool bDashing)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 보드는 여기서 다루지 않는다. 대시 키가 아니라 대시 동작(애님 상태 기계)을 따르므로
	// 애님 인스턴스가 SetBoardShown으로 세운다.

	if (!bDashing)
	{
		if (DashTrailComponent)
		{
			// 이미 태어난 파티클은 수명대로 사라지도록 새 스폰만 멈춘다.
			DashTrailComponent->Deactivate();
			DashTrailComponent = nullptr;
		}
		return;
	}

	if (DashTrailComponent)
	{
		return;
	}

	const FUnitActionFeedback* Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Dash) : nullptr;
	if (!Feedback)
	{
		return;
	}

	// 몽타주와 소리는 진입 순간의 일회성 연출이라 공용 경로를 그대로 쓴다.
	PlayFeedbackMontage(*Feedback);
	UGameAudioSubsystem::PlayAttached(AudioTags::Audio_Unit_Dash, GetRootComponent(), NAME_None, UnitData->Sounds);

	// 트레일만 따로 붙잡는다. 지속되는 이펙트라 끝날 때 직접 꺼야 하기 때문이다.
	if (Feedback->FX)
	{
		DashTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Feedback->FX,
			GetMesh(),
			Feedback->FXSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			// Deactivate 후 남은 파티클이 다 사라지면 스스로 정리된다. false로 두면
			// 대시할 때마다 꺼진 컴포넌트가 메시에 하나씩 쌓인다.
			true);
	}
}

void AUnit::PlayFeedbackMontage(const FUnitActionFeedback& Feedback)
{
	if (Feedback.Montage)
	{
		PlayAnimMontage(Feedback.Montage);
		return;
	}

	UAnimInstance* const AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Feedback.Animation || !AnimInstance)
	{
		return;
	}

	// 몽타주 에셋 없이 시퀀스를 슬롯에 얹는다. 그래프에 그 이름의 Slot 노드가 없으면 조용히 안 보인다.
	AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Feedback.Animation, Feedback.AnimationSlot, Feedback.AnimationBlendIn, Feedback.AnimationBlendOut);
}

void AUnit::HandleWeaponFired(int32 Seed)
{
	// 몸통 방향 게이트는 서버도 봐야 하므로 연출을 거르기 전에 적는다.
	if (const UWorld* const World = GetWorld())
	{
		LastFireTime = World->GetTimeSeconds();
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 연출 에셋이 없어도 한 발은 나갔으므로, 총은 Feedback 조회보다 먼저 꺼낸다.
	ShowGunForFire();

	const FUnitActionFeedback* Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Fire) : nullptr;
	if (!Feedback)
	{
		return;
	}

	PlayFeedbackMontage(*Feedback);

	// 발사음은 무기 컴포넌트가 총구에서 낸다(Audio.Weapon.Fire, 무기 프로필의 Sounds).

	// 총구 화염 같은 일회성 이펙트. 총에 Muzzle 소켓이 있으면 총구에서, 없으면 캐릭터 메시의
	// FXSocket에서 튼다. 둘 다 없으면 폰 위치에. 소켓 회전을 그대로 따르므로 총구 소켓의
	// 축이 총열 방향을 봐야 화염이 앞으로 뻗는다.
	if (Feedback->FX)
	{
		USceneComponent* AttachComponent = nullptr;
		FName AttachSocket = NAME_None;
		if (GunMesh && GunMesh->DoesSocketExist(GunMuzzleSocketName))
		{
			AttachComponent = GunMesh;
			AttachSocket = GunMuzzleSocketName;
		}
		else if (Feedback->FXSocket != NAME_None)
		{
			AttachComponent = GetMesh();
			AttachSocket = Feedback->FXSocket;
		}

		if (AttachComponent)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Feedback->FX, AttachComponent, AttachSocket,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Feedback->FX, GetActorLocation(), GetActorRotation());
		}
	}
}

void AUnit::HandleChargingChanged(bool bCharging)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bCharging)
	{
		// 쏘고 끝났든 취소됐든 총은 평소처럼 잠시 남았다가 들어간다. 실제로 한 발 나갔다면
		// HandleWeaponFired 가 곧 타이머를 다시 걸어 준다.
		StopChargePose();
		ShowGunForFire();
		return;
	}

	// 충전하는 동안 총은 계속 들려 있어야 한다. 유지 타이머를 걷어 두지 않으면 충전 도중에
	// 총이 사라지고, 그러면 발사 지점이 다시 쉬는 손으로 돌아간다.
	GetWorldTimerManager().ClearTimer(GunHideTimer);
	bGunVisible = true;
	UpdateGunVisibility();

	// 상체 자세는 UpperBody 슬롯이 정한다. 평소에는 이 슬롯으로 팔 내린 기본 포즈가 흐르고,
	// 발사할 때만 잠깐 발사 동작이 얹힌다. 충전은 놓을 때까지 이어지므로 그 사이 자세를
	// 붙들어 둘 것이 필요하다.
	StartChargePose();
}

void AUnit::StartChargePose()
{
	StopChargePose();

	const FUnitActionFeedback* const Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Charge) : nullptr;
	UAnimInstance* const AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Feedback || !Feedback->Animation || !AnimInstance)
	{
		// 충전 자세를 등록하지 않은 캐릭터는 지금까지처럼 기본 포즈로 충전한다.
		return;
	}

	// 슬롯 몽타주에는 “무한” 이 없다. 어떤 충전보다도 길게 돌 만큼만 반복해 두고, 실제로는
	// 방아쇠를 놓는 순간 StopChargePose 가 세운다.
	constexpr int32 LoopCount = 120;
	ChargePose = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Feedback->Animation, Feedback->AnimationSlot,
		Feedback->AnimationBlendIn, Feedback->AnimationBlendOut, /*InPlayRate=*/1.0f, LoopCount);
}

void AUnit::StopChargePose()
{
	UAnimMontage* const Pose = ChargePose.Get();
	ChargePose.Reset();
	if (!Pose)
	{
		return;
	}

	if (UAnimInstance* const AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		const FUnitActionFeedback* const Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Charge) : nullptr;
		const float BlendOut = Feedback ? Feedback->AnimationBlendOut : 0.15f;
		// 이 몽타주만 지목해서 세운다. 발사 동작이 이미 슬롯을 가져갔다면 아무 일도 일어나지 않는다.
		AnimInstance->Montage_Stop(BlendOut, Pose);
	}
}

void AUnit::ShowGunForFire()
{
	const float HoldTime = UnitData ? UnitData->GunVisibleHoldTime : 0.0f;
	if (!GunMesh || HoldTime <= 0.0f)
	{
		return;
	}

	bGunVisible = true;
	UpdateGunVisibility();

	// 연사 중에는 발사마다 타이머가 새로 걸려 총이 계속 남는다. 마지막 한 발에서만 실제로 만료된다.
	GetWorldTimerManager().SetTimer(GunHideTimer, this, &AUnit::HideGun, HoldTime, false);
}

void AUnit::HideGun()
{
	bGunVisible = false;
	UpdateGunVisibility();
}

void AUnit::UpdateGunVisibility()
{
	if (GunMesh)
	{
		// 카메라가 안에 들어와 몸이 반투명해진 동안에는 총도 감춘다. 페이드는 스켈레탈 메시의
		// 재질 슬롯만 바꾸므로(SetCameraFaded) 총만 불투명하게 남아 화면을 가린다.
		GunMesh->SetVisibility(bGunVisible && !bCameraFaded);
	}
}

void AUnit::SetBoardShown(bool bShown)
{
	if (bBoardShown == bShown)
	{
		return;
	}
	bBoardShown = bShown;
	UpdateBoardVisibility();
	UpdateBoardLoopSound();
}

void AUnit::UpdateBoardLoopSound()
{
	// 데디케이티드 서버에는 들을 사람이 없다. 나머지 머신은 각자 자기 화면의 보드를 따라 켠다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bBoardShown)
	{
		if (BoardAudioComponent)
		{
			BoardAudioComponent->Stop();
			BoardAudioComponent = nullptr;
		}
		return;
	}

	// 보드가 보이는 동안 하나만 돈다. 같은 보드에 두 번 켜지면 소리가 겹친다.
	if (BoardAudioComponent)
	{
		return;
	}

	BoardAudioComponent = UGameAudioSubsystem::PlayAttached(
		AudioTags::Audio_Unit_Board_Loop, GetRootComponent(), NAME_None, UnitData ? UnitData->Sounds : nullptr);
}

void AUnit::UpdateBoardVisibility()
{
	if (BoardMesh)
	{
		// 총과 같은 이유로 카메라 페이드 중에는 감춘다.
		BoardMesh->SetVisibility(bBoardShown && !bCameraFaded);
	}
}

void AUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnit, UnitData);

	// 소유자는 예측으로 이미 알고 있다. 보내면 자기가 아는 값을 한 번 더 받을 뿐이고,
	// 지연 때문에 오히려 예측을 되돌리게 된다.
	DOREPLIFETIME_CONDITION(AUnit, bIsDashing, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUnit, ReplicatedHeroPhase, COND_SkipOwner);
}

void AUnit::SetUnitData(UUnitDataAsset* NewUnitData)
{
	if (!HasAuthority() || UnitData == NewUnitData)
	{
		return;
	}

	UnitData = NewUnitData;

	// OnRep_UnitData는 변경을 수행한 권한 주체에서는 호출되지 않으므로,
	// 서버와 리슨 호스트의 메시는 여기서 직접 맞춰야 한다.
	ApplyUnitData();
}

void AUnit::OnRep_UnitData()
{
	ApplyUnitData();
}

void AUnit::ApplyUnitData()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!UnitData || !MeshComponent)
	{
		return;
	}

	// 페이드 중에 메시가 바뀌면 저장해 둔 원래 재질이 옛 메시 것이 된다. 풀었다가 다시 건다.
	const bool bWasCameraFaded = bCameraFaded;
	SetCameraFaded(false);

	// 메시 교체가 애님 인스턴스를 다시 만들기 때문에 순서를 바꾸면 애님 클래스가 날아간다.
	if (UnitData->Mesh)
	{
		MeshComponent->SetSkeletalMesh(UnitData->Mesh);
	}
	SetCameraFaded(bWasCameraFaded);

	if (UnitData->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(UnitData->AnimClass);
	}

	// 소켓 이름으로 다시 붙여야 교체된 메시의 소켓을 따라간다. 소켓이 없는 메시면
	// 메시 원점에 남으므로, 병이 발밑에 보이면 그 메시에 InkBottle 소켓이 빠진 것이다.
	if (InkBottle)
	{
		InkBottle->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("InkBottle"));
	}

	// 껍데기도 같은 메시로 맞추고 포즈를 넘겨받는다. 리더 포즈는 메시가 바뀔 때마다 다시
	// 걸어야 본 매핑이 새 메시를 따라간다.
	if (OutlineMesh && UnitData->Mesh)
	{
		OutlineMesh->SetSkeletalMesh(UnitData->Mesh);
		OutlineMesh->SetLeaderPoseComponent(MeshComponent);

		// 슬롯 수는 메시가 정하므로 메시를 넣은 뒤에 깐다.
		for (int32 Index = 0; Index < OutlineMesh->GetNumMaterials(); ++Index)
		{
			OutlineMesh->SetMaterial(Index, SuperArmorOutlineMaterial);
		}
	}

	// 메시가 바뀌면 총도 그 캐릭터의 것으로. Gun 소켓이 없는 메시면 병과 마찬가지로 발밑에 남는다.
	if (GunMesh)
	{
		GunMesh->SetStaticMesh(UnitData->GunMesh);
		GunMesh->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Gun"));
		UpdateGunVisibility();

		// 발사 지점도 총구로 옮긴다. 총 모양이 캐릭터마다 다르므로 소켓은 총 메시에 있고,
		// 총이나 Muzzle 소켓이 없으면 무기가 알아서 손 소켓으로 되돌아간다.
		for (UPaintWeaponComponent* const Weapon : { PaintWeapon.Get(), SecondaryWeapon.Get() })
		{
			if (Weapon)
			{
				Weapon->SetMuzzleSource(GunMesh, GunMuzzleSocketName);
			}
		}
	}

	// 보드도 그 캐릭터의 것으로. Board 소켓이 없는 메시면 발밑이 아니라 메시 원점에 남는다.
	if (BoardMesh)
	{
		BoardMesh->SetStaticMesh(UnitData->BoardMesh);
		BoardMesh->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Board"));
		UpdateBoardVisibility();
	}

	// 메시가 교체되면 오버레이도 새 메시에 다시 걸어야 한다.
	UpdateSuperArmorOutline();
}
