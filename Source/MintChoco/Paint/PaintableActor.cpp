#include "Paint/PaintableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Paint/PaintableComponent.h"

APaintableActor::APaintableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Find Collision UV reads the complex (per-triangle) representation, so traces have to hit
	// the render geometry rather than a simplified collision primitive.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->bAlwaysCreatePhysicsState = true;

	Paintable = CreateDefaultSubobject<UPaintableComponent>(TEXT("Paintable"));
}
