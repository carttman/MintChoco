// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/JumpPad.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/Unit.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(60.0f, 60.0f, 20.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldStatic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AJumpPad::OnTriggerBeginOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AJumpPad::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (!Unit)
	{
		return;
	}

	// 캐릭터 하나가 여러 컴포넌트로 겹칠 수 있다(등 뒤 잉크병 메시 등). 이동을 실제로
	// 담당하는 캡슐만 받아, 한 번 밟을 때 한 번만 발사되게 한다.
	if (OtherComp != Unit->GetCapsuleComponent())
	{
		return;
	}

	// 이 머신이 이 캐릭터의 움직임을 실제로 계산하는 쪽일 때만 발사한다. 다른 플레이어의
	// 복제본(시뮬레이션 프록시)에서 발사하면 서버가 보내오는 위치와 싸우게 된다.
	if (!Unit->IsLocallyControlled() && !HasAuthority())
	{
		return;
	}

	Unit->LaunchCharacter(FVector(0.0f, 0.0f, LaunchZSpeed), /*bXYOverride=*/false, /*bZOverride=*/true);
}
