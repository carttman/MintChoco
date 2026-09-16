#include "Items/ItemPickup.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"
#include "Game/Unit.h"
#include "Items/ItemLabelWidget.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"
#include "Items/ItemSpawnPoint.h"
#include "MintChoco.h"

float FItemPickupMotion::BobOffset(float Time, float Amplitude, float FrequencyHz)
{
	if (Amplitude <= 0.0f || FrequencyHz <= 0.0f)
	{
		return 0.0f;
	}
	return Amplitude * FMath::Sin(2.0f * PI * FrequencyHz * Time);
}

float FItemPickupMotion::SpinYaw(float Time, float RateDegPerSecond)
{
	return FRotator::NormalizeAxis(RateDegPerSecond * Time);
}

AItemPickup::AItemPickup()
{
	// 연출 틱. 활성 상태에서만 켠다(UpdateMotionEnabled).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// 점프대와 같은 레시피: 폰의 캡슐만 겹치게 하고, 나머지 채널은 모두 무시한다.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetSphereRadius(80.0f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetCollisionObjectType(ECC_WorldStatic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AItemPickup::OnTriggerBeginOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetVisibility(false);

	Laser = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Laser"));
	Laser->SetupAttachment(RootComponent);
	Laser->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Laser->SetGenerateOverlapEvents(false);
	Laser->SetCastShadow(false);

	// 스크린 공간이라 카메라를 따로 보지 않아도 늘 정면이고 글자 크기가 거리와 무관하다.
	Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetWidgetSpace(EWidgetSpace::Screen);
	Label->SetDrawAtDesiredSize(true);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetGenerateOverlapEvents(false);
	Label->SetVisibility(false);
	LabelWidgetClass = UItemLabelWidget::StaticClass();
}

void AItemPickup::Initialize(UItemProfile* InProfile, AItemSpawnPoint* InSpawnPoint, float InWarningTime)
{
	Profile = InProfile;
	SpawnPoint = InSpawnPoint;
	WarningTime = InWarningTime;
	if (InSpawnPoint)
	{
		InSpawnPoint->CurrentPickup = this;
	}
}

AItemPickup* AItemPickup::SpawnAt(UWorld& World, UClass* PickupClass, AItemSpawnPoint& Point, UItemProfile& Item, float WarningTime, AActor* Owner)
{
	if (!PickupClass) return nullptr;

	const FTransform Transform = Point.GetActorTransform();
	AItemPickup* const Pickup = World.SpawnActorDeferred<AItemPickup>(
		PickupClass, Transform, Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->Initialize(&Item, &Point, WarningTime);
	Pickup->FinishSpawning(Transform);
	return Pickup;
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();

	// BP가 정한 메시 자리를 기준으로 흔든다. 틱이 덮어쓰기 전에 읽어 둔다.
	if (Mesh)
	{
		MeshBaseLocation = Mesh->GetRelativeLocation();
		MeshBaseRotation = Mesh->GetRelativeRotation();
	}

	if (Label)
	{
		Label->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight));
	}

	ApplyProfile();
	ApplyState();

	// 예고음은 태어날 때 한 번. ApplyState는 여러 번 불리므로(BeginPlay, OnRep) 거기 두지 않는다.
	// 액터가 복제되어 머신마다 BeginPlay를 지나므로 각자 한 번씩이다.
	if (State == EItemPickupState::Announced)
	{
		UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_Item_Announce, GetActorLocation());
	}

	if (HasAuthority())
	{
		if (WarningTime > 0.0f)
		{
			GetWorldTimerManager().SetTimer(ActivateTimer, this, &AItemPickup::Activate, WarningTime, /*bLoop=*/false);
		}
		else
		{
			Activate();
		}
	}
}

void AItemPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Mesh)
	{
		return;
	}
	MotionTime += DeltaTime;
	Mesh->SetRelativeLocation(MeshBaseLocation + FVector(0.0f, 0.0f, FItemPickupMotion::BobOffset(MotionTime, BobAmplitude, BobFrequency)));
	FRotator Rotation = MeshBaseRotation;
	Rotation.Yaw = FRotator::NormalizeAxis(MeshBaseRotation.Yaw + FItemPickupMotion::SpinYaw(MotionTime, SpinRateDeg));
	Mesh->SetRelativeRotation(Rotation);
}

void AItemPickup::UpdateMotionEnabled()
{
	SetActorTickEnabled(State == EItemPickupState::Active && !bCollected);
}

void AItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AItemPickup, Profile);
	DOREPLIFETIME(AItemPickup, State);
	DOREPLIFETIME(AItemPickup, bCollected);
}

void AItemPickup::Activate()
{
	if (State == EItemPickupState::Active)
	{
		return;
	}

	State = EItemPickupState::Active;
	// RepNotify는 권한 쪽에서 불리지 않으므로 서버(리슨 호스트)도 직접 반영한다.
	ApplyState();
}

void AItemPickup::OnRep_Profile()
{
	ApplyProfile();
}

void AItemPickup::OnRep_State()
{
	ApplyState();
}

void AItemPickup::OnRep_Collected()
{
	UpdateMotionEnabled();
	if (bCollected)
	{
		SetActorHiddenInGame(true);
		// 서버는 OnTriggerBeginOverlap이 직접 부르고 클라이언트는 복제로 온다: 머신마다 한 번.
		UGameAudioSubsystem::PlayAt(this, AudioTags::Audio_Item_Pickup, GetActorLocation(), Profile ? Profile->Sounds.Get() : nullptr);
	}
}

void AItemPickup::ApplyProfile()
{
	// 박스는 종류와 무관하게 같은 모양(BP_ItemPickup의 Egg 메시)이다. 종류는 HUD로 구분하고 이름표는 디버그용이다.
	// 프로필이 상태보다 늦게 복제돼도 이름표가 켜진다.
	UpdateLabel();
}

void AItemPickup::UpdateLabel()
{
	if (!Label) return;

	const bool bVisible = IsLabelEnabled() && State == EItemPickupState::Active && Profile != nullptr;
	if (bVisible && !Label->GetUserWidgetObject())
	{
		// 위젯은 처음 보일 때 만든다. 에디터 밖에서는 끝까지 만들지 않는다.
		Label->SetWidgetClass(LabelWidgetClass);
		Label->InitWidget();
	}

	if (Profile)
	{
		if (UItemLabelWidget* const Widget = Cast<UItemLabelWidget>(Label->GetUserWidgetObject()))
		{
			const FText Name = Profile->DisplayName.IsEmpty() ? FText::FromString(Profile->GetName()) : Profile->DisplayName;
			Widget->SetLabel(Name);
		}
	}
	Label->SetVisibility(bVisible);
}

bool AItemPickup::IsLabelEnabled() const
{
#if WITH_EDITOR
	const UWorld* const World = GetWorld();
	return bShowLabel && World && World->IsPlayInEditor();
#else
	return false;
#endif
}

void AItemPickup::ApplyState()
{
	const bool bActive = State == EItemPickupState::Active;

	if (Mesh)
	{
		Mesh->SetVisibility(bActive);
	}
	UpdateAura(bActive);
	if (Laser)
	{
		Laser->SetVisibility(!bActive);
	}
	UpdateLabel();
	if (Trigger)
	{
		Trigger->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	UpdateMotionEnabled();

	BP_OnStateChanged(State);
}

void AItemPickup::UpdateAura(bool bActive)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bActive || !AuraFX)
	{
		if (AuraFXComponent)
		{
			// 대시 트레일과 같다. 새 스폰만 멈추고 떠 있던 입자는 제 수명대로 사라진다.
			// 습득은 이 경로가 아니다: 그쪽은 액터를 통째로 숨기므로(OnRep_Collected) 오라도 같이 걷힌다.
			AuraFXComponent->Deactivate();
			AuraFXComponent = nullptr;
		}
		return;
	}

	if (AuraFXComponent)
	{
		return;
	}

	// 메시에 붙인다. 박스가 떠다니고 도는 것을 오라가 그대로 따라간다.
	AuraFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		AuraFX,
		Mesh,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		// Deactivate 뒤 남은 입자가 다 사라지면 스스로 정리된다.
		true);
}

void AItemPickup::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 습득은 서버만 정한다. 동시에 밟아도 첫 오버랩 하나만 통과한다.
	if (!HasAuthority() || bCollected || State != EItemPickupState::Active)
	{
		return;
	}

	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (!Unit || OtherComp != Unit->GetCapsuleComponent())
	{
		return;
	}

	UItemSlotComponent* const Slot = Unit->GetItemSlot();
	if (!Slot || !Profile)
	{
		return;
	}

	Slot->GiveItem(Profile);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s picked up %s."), *GetNameSafe(Unit), *GetNameSafe(Profile));

	bCollected = true;
	OnRep_Collected();
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SpawnPoint.IsValid() && SpawnPoint->CurrentPickup == this)
	{
		SpawnPoint->CurrentPickup = nullptr;
	}

	// 바로 지우면 클라이언트가 소리를 낼 복제가 도착하지 못할 수 있어 잠시 남긴다.
	SetLifeSpan(1.0f);
}
