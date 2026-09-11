#include "Items/ItemPickup.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Game/Unit.h"
#include "Items/ItemLabelWidget.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSlotComponent.h"
#include "Items/ItemSpawnPoint.h"
#include "MintChoco.h"

AItemPickup::AItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;
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

	if (Label)
	{
		Label->SetWidgetClass(bShowLabel ? LabelWidgetClass : nullptr);
		Label->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight));
		Label->InitWidget();
	}

	ApplyProfile();
	ApplyState();

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
	if (bCollected)
	{
		SetActorHiddenInGame(true);
		if (Profile && Profile->PickupSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Profile->PickupSound, GetActorLocation());
		}
	}
}

void AItemPickup::ApplyProfile()
{
	// 박스는 종류와 무관하게 같은 모양(BP_ItemPickup의 Egg 메시)이다. 종류는 이름표와 HUD로만 구분한다.
	if (!Label || !Profile) return;

    if (UItemLabelWidget* const Widget = Cast<UItemLabelWidget>(Label->GetUserWidgetObject()))
	{
		const FText Name = Profile->DisplayName.IsEmpty() ? FText::FromString(Profile->GetName()) : Profile->DisplayName;
		Widget->SetLabel(Name);
	}
	// 프로필이 상태보다 늦게 복제돼도 이름표가 켜진다.
	Label->SetVisibility(State == EItemPickupState::Active && bShowLabel);
}

void AItemPickup::ApplyState()
{
	const bool bActive = State == EItemPickupState::Active;

	if (Mesh)
	{
		Mesh->SetVisibility(bActive);
	}
	if (Laser)
	{
		Laser->SetVisibility(!bActive);
	}
	if (Label)
	{
		Label->SetVisibility(bActive && bShowLabel && Profile != nullptr);
	}
	if (Trigger)
	{
		Trigger->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	BP_OnStateChanged(State);
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
