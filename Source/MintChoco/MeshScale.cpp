#include "MeshScale.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void ScaleMeshToRadius(UStaticMeshComponent* Mesh, float Radius)
{
	const UStaticMesh* const Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (!Asset)
	{
		return;
	}

	const float MeshRadius = Asset->GetBounds().SphereRadius;
	if (MeshRadius > KINDA_SMALL_NUMBER)
	{
		Mesh->SetRelativeScale3D(FVector(Radius / MeshRadius));
	}
}
