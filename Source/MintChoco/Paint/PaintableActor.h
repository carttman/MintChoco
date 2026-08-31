#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PaintableActor.generated.h"

class UPaintableComponent;
class UStaticMeshComponent;

/**
 * A static mesh that can be painted on any face. Blueprint subclasses pick the mesh and the
 * materials; nothing here knows or cares what shape it is.
 */
UCLASS()
class MINTCHOCO_API APaintableActor : public AActor
{
	GENERATED_BODY()

public:
	APaintableActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UPaintableComponent> Paintable;
};
