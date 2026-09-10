// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Unit.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
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
#include "Weapons/PaintWeaponComponent.h"

AUnit::AUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUnitMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// 유닛 자체가 매 프레임 할 일은 없다. 이동은 CharacterMovement가 돌리고,
	// 발사와 스킬은 각자의 컴포넌트가 필요한 동안만 틱한다.
	PrimaryActorTick.bCanEverTick = false;

	// 캐릭터가 항상 카메라를 바라본다.
	//
	// 조준 방향과 캐릭터 정면이 일치해야 총구가 화면 중앙을 향한다. 페인트 총은
	// 움직이면서 쏘는 것이 기본 동작이라, 이동 방향을 바라보게 두면 옆으로 달리며
	// 쏠 때마다 총이 몸을 통과한다. 대가로 옆·뒤로 걷는 스트레이프 애니메이션이
	// 필요하다.
	bUseControllerRotationYaw = true;
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

void AUnit::ApplyViewPitchLimits()
{
	// 카메라 매니저는 로컬 플레이어 컨트롤러에만 있다. 데디케이티드 서버와 원격 폰에서는
	// 걸 대상이 없고, 걸 필요도 없다: 회전은 소유 클라이언트가 만들어 보낸다.
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (APlayerCameraManager* const CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr)
	{
		CameraManager->ViewPitchMin = ViewPitchMin;
		CameraManager->ViewPitchMax = ViewPitchMax;
	}
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
		return;
	}

	for (int32 Index = 0; Index < CameraFadeOriginalMaterials.Num(); ++Index)
	{
		MeshComponent->SetMaterial(Index, CameraFadeOriginalMaterials[Index]);
	}
	CameraFadeOriginalMaterials.Reset();
	bCameraFaded = false;
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

bool AUnit::IsMovementInputLocked() const
{
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
	BP_OnStunned(bStunned);
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

	// 소유 클라이언트에서는 입력이, 서버에서는 압축 플래그가 이 알림을 낸다.
	// 어느 쪽이든 실제로 상태가 바뀔 때만 한 번씩 온다.
	if (UUnitMovementComponent* Movement = GetUnitMovement())
	{
		Movement->OnDashStateChanged.AddUObject(this, &AUnit::HandleDashStateChanged);
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

void AUnit::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

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

	// 여기가 로컬 조종 폰이 확정되는 유일한 지점이라 시야 한계도 같이 넣는다. 리스폰하면
	// 이 함수가 다시 불리므로 새 카메라 매니저에도 자동으로 다시 걸린다.
	ApplyViewPitchLimits();

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
	// 서버만 다른 클라이언트에게 알릴 수 있다. 소유 클라이언트는 자기 예측으로
	// 이미 알고 있으므로 복제에서 제외되어 있다.
	if (HasAuthority())
	{
		bIsDashing = bDashing;
	}

	UpdateDashEffects(bDashing);
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

	if (Feedback->Sound)
	{
		UGameplayStatics::SpawnSoundAttached(Feedback->Sound, GetRootComponent());
	}

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
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FUnitActionFeedback* Feedback = UnitData ? UnitData->FindFeedback(EUnitAction::Fire) : nullptr;
	if (!Feedback)
	{
		return;
	}

	PlayFeedbackMontage(*Feedback);

	if (Feedback->Sound)
	{
		UGameplayStatics::SpawnSoundAttached(Feedback->Sound, GetRootComponent());
	}

	// 총구 화염 같은 일회성 이펙트. 소켓이 없으면 폰 위치에.
	if (Feedback->FX)
	{
		if (Feedback->FXSocket != NAME_None)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Feedback->FX, GetMesh(), Feedback->FXSocket,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Feedback->FX, GetActorLocation(), GetActorRotation());
		}
	}
}

void AUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnit, UnitData);

	// 소유자는 예측으로 이미 알고 있다. 보내면 자기가 아는 값을 한 번 더 받을 뿐이고,
	// 지연 때문에 오히려 예측을 되돌리게 된다.
	DOREPLIFETIME_CONDITION(AUnit, bIsDashing, COND_SkipOwner);
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
}
