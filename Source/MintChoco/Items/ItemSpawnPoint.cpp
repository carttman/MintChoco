#include "Items/ItemSpawnPoint.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include "Items/ItemPickup.h"
#include "Items/ItemProfile.h"
#include "Items/ItemSettings.h"
#include "MintChoco.h"

AItemSpawnPoint::AItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
	}
#endif
}

void AItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && SpawnMode == EItemSpawnMode::Standalone)
	{
		SpawnStandalone(0.0f);
	}
}

UItemProfile* AItemSpawnPoint::ChooseItem(const FRandomStream& Random) const
{
	if (FixedItem) return FixedItem;

	TArray<UItemProfile*> Items;
	UItemSettings::Get().LoadItems(Items);

	return Items.IsEmpty() ? nullptr : Items[Random.RandRange(0, Items.Num() - 1)];
}

void AItemSpawnPoint::SpawnStandalone(float WarningTime)
{
	UItemProfile* const Item = ChooseItem(FRandomStream(FMath::Rand()));
	UClass* const PickupClass = UItemSettings::Get().LoadPickupClass();
	if (!Item || !PickupClass)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: 놓을 아이템이 없다(아이템 %s, 픽업 클래스 %s)."),
			*GetName(), Item ? TEXT("있음") : TEXT("없음"), PickupClass ? TEXT("있음") : TEXT("없음"));
		return;
	}

	AItemPickup* const Pickup = AItemPickup::SpawnAt(*GetWorld(), PickupClass, *this, *Item, WarningTime, this);
	if (Pickup)
	{
		Pickup->OnDestroyed.AddDynamic(this, &AItemSpawnPoint::HandlePickupDestroyed);
	}
}

void AItemSpawnPoint::HandlePickupDestroyed(AActor* DestroyedActor)
{
	// 가져간 픽업은 잠시 뒤 사라진다. 그때부터 예고를 세우고 RespawnDelay 뒤 다음 것이 나온다.
	UWorld* const World = GetWorld();
	if (!World || World->bIsTearingDown || !HasAuthority() || SpawnMode != EItemSpawnMode::Standalone || !IsFree())
	{
		return;
	}
	SpawnStandalone(RespawnDelay);
}
