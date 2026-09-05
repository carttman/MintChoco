// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Unit.h"

#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/GamePlayerState.h"
#include "Game/TeamTypes.h"
#include "Game/UnitInputConfig.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "MintChoco.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "Weapons/PaintWeaponComponent.h"

AUnit::AUnit()
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

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;

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

	PaintWeapon = CreateDefaultSubobject<UPaintWeaponComponent>(TEXT("PaintWeapon"));
}

void AUnit::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyTeamToWeapon();
}

void AUnit::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyTeamToWeapon();
}

void AUnit::ApplyTeamToWeapon()
{
	// 팀 번호가 곧 페인트 id다(민트 0, 초코 1). 팀이 없는 PlayerState(샘플 맵)는
	// 건드리지 않아, 다른 곳에서 정해 준 id가 남는다.
	const AGamePlayerState* GamePlayerState = GetPlayerState<AGamePlayerState>();
	if (PaintWeapon && GamePlayerState && Teams::IsValidId(GamePlayerState->GetTeam()))
	{
		PaintWeapon->SetPaintId(static_cast<uint8>(GamePlayerState->GetTeam()));
	}
}

void AUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BeginPlay보다 이른 시점이라 첫 프레임이 그려지기 전에 메시가 확정된다.
	// 여기서 보이는 값은 클라이언트에서도 블루프린트 기본값이므로, 런타임에
	// 교체된 경우는 OnRep_UnitData가 뒤이어 처리한다.
	ApplyUnitData();
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

	// 연사와 붓은 누르고 있는 동안 계속 나가야 하므로 놓는 쪽도 묶는다.
	// Canceled는 다른 입력이 이 액션을 가로챘을 때이며, 그때도 방아쇠는 놓여야 한다.
	if (InputConfig->FireAction)
	{
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Started, this, &AUnit::StartFire);
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Completed, this, &AUnit::StopFire);
		EnhancedInput->BindAction(InputConfig->FireAction, ETriggerEvent::Canceled, this, &AUnit::StopFire);
	}
}

void AUnit::StartFire()
{
	PaintWeapon->PullTrigger();
}

void AUnit::StopFire()
{
	PaintWeapon->ReleaseTrigger();
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

	Super::EndPlay(EndPlayReason);
}

void AUnit::Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (MoveInput.IsNearlyZero())
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

void AUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnit, UnitData);
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

	// 메시 교체가 애님 인스턴스를 다시 만들기 때문에 순서를 바꾸면 애님 클래스가 날아간다.
	if (UnitData->Mesh)
	{
		MeshComponent->SetSkeletalMesh(UnitData->Mesh);
	}

	if (UnitData->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(UnitData->AnimClass);
	}
}

float AUnit::PlayActionFeedback(EUnitAction Action)
{
	return PlayActionFeedbackAtLocation(Action, GetActorLocation());
}

float AUnit::PlayActionFeedbackAtLocation(EUnitAction Action, const FVector& WorldLocation)
{
	const FUnitActionFeedback* Feedback = UnitData ? UnitData->FindFeedback(Action) : nullptr;
	if (!Feedback)
	{
		return 0.0f;
	}

	// 몽타주는 루트 모션과 AnimNotify를 통해 게임플레이에 영향을 주므로 서버에서도 돈다.
	float Duration = 0.0f;
	if (Feedback->Montage)
	{
		Duration = PlayAnimMontage(Feedback->Montage);
	}

	// 이펙트와 소리는 순수 연출이라 데디케이티드 서버에서는 낭비다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return Duration;
	}

	if (Feedback->FX)
	{
		if (Feedback->FXSocket.IsNone())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, Feedback->FX, WorldLocation, GetActorRotation());
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Feedback->FX,
				GetMesh(),
				Feedback->FXSocket,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true);
		}
	}

	if (Feedback->Sound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Feedback->Sound, WorldLocation);
	}

	return Duration;
}
