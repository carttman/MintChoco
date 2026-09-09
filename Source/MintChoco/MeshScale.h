#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;

/** Scales the component so the bounding sphere of its mesh shows the given radius in cm. */
void ScaleMeshToRadius(UStaticMeshComponent* Mesh, float Radius);
