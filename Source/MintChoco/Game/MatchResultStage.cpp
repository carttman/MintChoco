#include "Game/MatchResultStage.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"

#include "Game/MatchResultSettings.h"
#include "Game/UnitDataAsset.h"
#include "MintChoco.h"

namespace MatchResultStage
{
	/** 카메라에서 캐릭터 자리까지. 가까울수록 캐릭터가 크게 잡히고 맵을 많이 가린다. */
	constexpr float SlotDistanceCm = 520.0f;

	/** 화면 중앙에서 좌우 자리까지. */
	constexpr float SlotSpreadCm = 210.0f;

	/**
	 * 자리의 높낮이. Lvl_Stage에서 맞춰 본 값이다: 카메라에서 520 cm 앞, FOV 65, 16:9에서 발끝이
	 * 화면 아래 0.75 지점에 와 결과 게이지 띠와 정확히 겹친다.
	 */
	constexpr float SlotHeightCm = -140.0f;

	/**
	 * 자리를 카메라 쪽으로 돌려 두는 각. 에디터에서 메시를 켜 두고 구도를 볼 때를 위한 것일 뿐,
	 * 런타임 방향은 PoseAtOffset 이 카메라를 보고 다시 계산하므로 이 값에 기대지 않는다.
	 */
	constexpr float SlotFacingYaw = 180.0f;
}

AMatchResultStage::AMatchResultStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 레벨에 놓인 액터라 머신마다 자기 것이 이미 있다. 연출은 전부 로컬이므로 복제할 것이 없다.
	bReplicates = false;
	SetCanBeDamaged(false);

	// 뷰 타깃이 되었을 때 AActor::CalcCamera가 아래 카메라 컴포넌트를 찾아 쓰게 한다.
	bFindCameraComponentWhenViewTarget = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);

	LeftSlot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftSlot"));
	RightSlot = CreateDefaultSubobject<USceneComponent>(TEXT("RightSlot"));
	LeftCharacter = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftCharacter"));
	RightCharacter = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightCharacter"));

	LayOutSlot(LeftSlot, LeftCharacter, -1.0f);
	LayOutSlot(RightSlot, RightCharacter, 1.0f);
}

void AMatchResultStage::LayOutSlot(USceneComponent* Slot, USkeletalMeshComponent* Character, float Side)
{
	Slot->SetupAttachment(Camera);
	Slot->SetRelativeLocation(FVector(
		MatchResultStage::SlotDistanceCm, Side * MatchResultStage::SlotSpreadCm, MatchResultStage::SlotHeightCm));
	Slot->SetRelativeRotation(FRotator(0.0f, MatchResultStage::SlotFacingYaw, 0.0f));

	Character->SetupAttachment(Slot);
	Character->SetRelativeRotation(FRotator(0.0f, MeshYawOffset, 0.0f));
	Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->SetGenerateOverlapEvents(false);
	// 맵에서 한참 떠 있는 전경 요소다. 그림자를 드리우면 바닥에 정체 모를 검은 덩어리가 생긴다.
	Character->SetCastShadow(false);
	Character->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Character->SetVisibility(false);
}

void AMatchResultStage::BeginPlay()
{
	Super::BeginPlay();

	// 회색조는 이 카메라에만 얹는다. 경기 중 화면은 이 포스트프로세스를 거치지 않는다.
	const UMatchResultSettings& Settings = UMatchResultSettings::Get();
	if (UMaterialInterface* const Desaturate = Settings.LoserDesaturateMaterial.LoadSynchronous())
	{
		Camera->PostProcessSettings.AddBlendable(Desaturate, 1.0f);
	}
}

void AMatchResultStage::Prepare(const FMatchResultSlotCast& Left, const FMatchResultSlotCast& Right)
{
	Dress(LeftCharacter, LeftSlot, LeftState, Left);
	Dress(RightCharacter, RightSlot, RightState, Right);
	MoveRemaining = 0.0f;
	SetActorTickEnabled(false);
}

void AMatchResultStage::PlayFinish(int32 WinningTeam)
{
	const UMatchResultSettings& Settings = UMatchResultSettings::Get();

	const auto Aim = [&Settings, WinningTeam](FSlotState& State)
	{
		State.Step = FMatchResultMath::GetStep(State.Team, WinningTeam);
		switch (State.Step)
		{
		case EMatchResultStep::Approach: State.TargetOffset = Settings.WinnerStepCm; break;
		case EMatchResultStep::Withdraw: State.TargetOffset = -Settings.LoserStepCm; break;
		default:                         State.TargetOffset = 0.0f; break;
		}
		State.bFinishPending = true;
	};
	Aim(LeftState);
	Aim(RightState);

	MoveRemaining = FMath::Max(Settings.CharacterSeconds, 0.0f);
	SetActorTickEnabled(true);

	// 움직이지 않는 마무리(무승부, 또는 이동 시간이 0)는 틱을 기다릴 것이 없다.
	if (MoveRemaining <= 0.0f)
	{
		AdvanceSlot(LeftState, LeftCharacter, LeftSlot, 0.0f);
		AdvanceSlot(RightState, RightCharacter, RightSlot, 0.0f);
	}
}

void AMatchResultStage::Dismiss()
{
	SetActorTickEnabled(false);
	MoveRemaining = 0.0f;

	const auto Clear = [this](USkeletalMeshComponent* Character, FSlotState& State)
	{
		State = FSlotState();
		if (!Character)
		{
			return;
		}
		Character->SetVisibility(false);
		Character->bPauseAnims = false;
		Character->SetRenderCustomDepth(false);
		Character->SetRelativeLocation(FVector::ZeroVector);
		Character->SetRelativeRotation(FRotator(0.0f, MeshYawOffset, 0.0f));
		Character->SetSkeletalMeshAsset(nullptr);
	};
	Clear(LeftCharacter, LeftState);
	Clear(RightCharacter, RightState);
}

void AMatchResultStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AdvanceSlot(LeftState, LeftCharacter, LeftSlot, DeltaSeconds);
	AdvanceSlot(RightState, RightCharacter, RightSlot, DeltaSeconds);

	MoveRemaining = FMath::Max(MoveRemaining - DeltaSeconds, 0.0f);
	if (!LeftState.bFinishPending && !RightState.bFinishPending)
	{
		SetActorTickEnabled(false);
	}
}

void AMatchResultStage::FrameBounds(const FBox& Bounds)
{
	if (!Bounds.IsValid)
	{
		return;
	}

	// 가로 화각으로 재고 여유를 곱한다. 대충 잡는 자리라 정확할 필요는 없고, 잘리지만 않으면 된다.
	const float HalfAngle = FMath::DegreesToRadians(FMath::Clamp(Camera->FieldOfView, 10.0f, 170.0f) * 0.5f);
	const float Radius = FMath::Max(Bounds.GetExtent().Size(), 1.0f);
	const float Distance = Radius * FallbackMargin / FMath::Max(FMath::Tan(HalfAngle), UE_KINDA_SMALL_NUMBER);

	const FRotator Look(FallbackPitchDegrees, GetActorRotation().Yaw, 0.0f);
	SetActorLocationAndRotation(Bounds.GetCenter() - Look.Vector() * Distance, Look);

	UE_LOG(LogMintChoco, Log, TEXT("결과 무대: 맵에 놓인 것이 없어 페인트 영역(반지름 %.0f cm)에서 구도를 잡았다."), Radius);
}

void AMatchResultStage::Dress(USkeletalMeshComponent* Character, const USceneComponent* Slot, FSlotState& State,
	const FMatchResultSlotCast& Cast) const
{
	State = FSlotState();
	State.Team = Cast.Team;

	if (!Character)
	{
		return;
	}

	USkeletalMesh* const Mesh = Cast.UnitData ? Cast.UnitData->Mesh.Get() : nullptr;
	Character->bPauseAnims = false;
	Character->SetRenderCustomDepth(false);
	PoseAtOffset(State, Character, Slot);
	Character->SetSkeletalMeshAsset(Mesh);
	Character->SetVisibility(Mesh != nullptr);

	if (!Mesh)
	{
		UE_LOG(LogMintChoco, Warning,
			TEXT("결과 무대: %s 팀의 캐릭터 정의가 없어 그 자리를 비운다. BP_GameMode의 TeamUnitData를 확인한다."),
			Teams::GetDisplayName(Cast.Team));
		return;
	}

	// 애님 블루프린트를 붙이지 않는다. 폰에서 값을 읽는 그래프라 폰 없는 표시용 메시에서는 의미가 없고,
	// 단일 노드 재생이 "서 있기 → 춤 → 완전 정지"를 그대로 표현한다.
	PlayLooping(Character, UMatchResultSettings::Get().IdleAnimation.LoadSynchronous());
}

void AMatchResultStage::AdvanceSlot(FSlotState& State, USkeletalMeshComponent* Character, const USceneComponent* Slot,
	float DeltaSeconds) const
{
	if (!State.bFinishPending || !Character)
	{
		return;
	}

	// 남은 시간에 대한 비율로 좁힌다. 매 프레임 목표까지의 나머지를 나누므로 딱 시간 안에 도착한다.
	const float Alpha = MoveRemaining > DeltaSeconds ? DeltaSeconds / MoveRemaining : 1.0f;
	State.Offset = Alpha < 1.0f ? FMath::Lerp(State.Offset, State.TargetOffset, Alpha) : State.TargetOffset;
	PoseAtOffset(State, Character, Slot);

	if (Alpha < 1.0f)
	{
		return;
	}

	State.bFinishPending = false;
	ApplyFinish(State, Character);
}

void AMatchResultStage::PoseAtOffset(const FSlotState& State, USkeletalMeshComponent* Character, const USceneComponent* Slot) const
{
	if (!Character || !Slot)
	{
		return;
	}

	// 자리의 회전은 쓰지 않는다. 자리는 위치만 뜻한다.
	const FTransform Pose = FMatchResultMath::MakeCharacterTransform(
		Camera->GetComponentTransform(), Slot->GetComponentLocation(), State.Offset, MeshYawOffset);
	Character->SetWorldLocationAndRotation(Pose.GetLocation(), Pose.GetRotation());
}

void AMatchResultStage::ApplyFinish(const FSlotState& State, USkeletalMeshComponent* Character) const
{
	const UMatchResultSettings& Settings = UMatchResultSettings::Get();

	switch (State.Step)
	{
	case EMatchResultStep::Approach:
		PlayLooping(Character, Settings.WinnerAnimation.LoadSynchronous());
		return;

	case EMatchResultStep::Withdraw:
		// 틱만 멈추고 본은 그대로 갱신되므로 마지막 자세에서 얼어붙는다.
		Character->bPauseAnims = true;
		Character->SetCustomDepthStencilValue(Settings.LoserStencilValue);
		Character->SetRenderCustomDepth(true);
		return;

	default:
		// 무승부. 서 있는 것을 그대로 둔다.
		return;
	}
}

void AMatchResultStage::PlayLooping(USkeletalMeshComponent* Character, UAnimSequence* Animation)
{
	if (!Character || !Animation)
	{
		return;
	}
	Character->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Character->PlayAnimation(Animation, /*bLooping=*/true);
}
