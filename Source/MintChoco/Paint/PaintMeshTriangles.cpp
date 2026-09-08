#include "Paint/PaintMeshTriangles.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"

#include "Paint/PaintLog.h"

bool GatherPaintMeshTriangles(const UStaticMeshComponent& Mesh, int32 MaterialSlot, FPaintMeshTriangles& Out)
{
	const FString Owner = Mesh.GetReadableName();
	const UStaticMesh* const Asset = Mesh.GetStaticMesh();
	const FStaticMeshRenderData* const RenderData = Asset ? Asset->GetRenderData() : nullptr;
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: no render data on the mesh, paint disabled."), *Owner);
		return false;
	}

	// LOD 0 is the Nanite fallback on a Nanite mesh: exact for flat art, a little coarse on curves.
	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionBuffer = LOD.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	const FIndexArrayView IndexView = LOD.IndexBuffer.GetArrayView();
	const uint32 VertexCount = PositionBuffer.GetNumVertices();
	if (VertexCount == 0 || IndexView.Num() == 0)
	{
		UE_LOG(LogPaint, Warning,
			TEXT("%s: mesh geometry is not CPU-readable (enable Allow CPU Access on %s), paint disabled."),
			*Owner, *GetNameSafe(Asset));
		return false;
	}

	// Only the slot that shows paint counts; a second material on the mesh is not paintable.
	Out.Indices.Reset();
	for (const FStaticMeshSection& Section : LOD.Sections)
	{
		const int32 End = Section.FirstIndex + Section.NumTriangles * 3;
		if (Section.MaterialIndex != MaterialSlot || End > IndexView.Num())
		{
			continue;
		}
		Out.Indices.Reserve(Out.Indices.Num() + Section.NumTriangles * 3);
		for (int32 Index = Section.FirstIndex; Index < End; ++Index)
		{
			Out.Indices.Add(IndexView[Index]);
		}
	}
	if (Out.Indices.IsEmpty())
	{
		UE_LOG(LogPaint, Warning, TEXT("%s: material slot %d has no triangles, paint disabled."), *Owner, MaterialSlot);
		return false;
	}

	Out.Positions = TArray<FVector3f>(static_cast<const FVector3f*>(PositionBuffer.GetVertexData()), VertexCount);

	Out.Normals.Reset();
	if (VertexBuffer.GetNumVertices() == VertexCount)
	{
		Out.Normals.SetNumUninitialized(VertexCount);
		for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
		{
			Out.Normals[Vertex] = FVector3f(VertexBuffer.VertexTangentZ(Vertex));
		}
	}
	return true;
}
