#include "Paint/PaintableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Paint/PaintableComponent.h"

APaintableActor::APaintableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Paint sources find surfaces by trace and overlap, so the mesh must answer queries even
	// when nothing else in the level would ever collide with it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->bAlwaysCreatePhysicsState = true;

	Paintable = CreateDefaultSubobject<UPaintableComponent>(TEXT("Paintable"));
}
