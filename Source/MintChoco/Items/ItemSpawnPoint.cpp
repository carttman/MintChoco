#include "Items/ItemSpawnPoint.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

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
