#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/CoreMiscDefines.h"

/**
 * 액터를 스폰해야 하는 자동화 테스트를 위한 임시 게임 월드.
 *
 * 지오메트리도 컨트롤러도 없다: 필요한 것은 액터가 태어나고 BeginPlay가 도는 것뿐이다.
 * 다 쓰면 반드시 DestroyWorld까지 가야 다음 테스트가 깨끗한 월드에서 시작한다.
 */
namespace MintChocoTest
{
	inline UWorld* MakeWorld()
	{
		UWorld* const World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}

		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		return World;
	}

	/**
	 * 막고 밟히는 상자 하나. 조준 트레이스와 착지 판정에 쓸 바닥·벽·단을 세운다.
	 *
	 * Center는 상자의 중심이므로 윗면은 Center.Z + HalfExtent.Z다. 정적 월드 오브젝트라
	 * 시야 트레이스(ECC_Visibility)에도 걸린다.
	 */
	inline AActor* SpawnBlock(UWorld& World, const FVector& Center, const FVector& HalfExtent)
	{
		AActor* const Block = World.SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator);
		if (!Block)
		{
			return nullptr;
		}

		UBoxComponent* const Box = NewObject<UBoxComponent>(Block);
		Block->SetRootComponent(Box);
		Box->SetBoxExtent(HalfExtent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Block->SetActorLocation(Center);
		return Block;
	}

	inline void DestroyWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}
}
