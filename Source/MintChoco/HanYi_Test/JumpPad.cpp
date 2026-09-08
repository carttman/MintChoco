// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/JumpPad.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->InitBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetRootComponent(TriggerBox);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(RootComponent);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.25f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeMeshFinder.Object);
	}
}

void AJumpPad::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AJumpPad::OnTriggerOverlap);
}

FVector AJumpPad::CalculateLaunchVelocity() const
{
	const UWorld* World = GetWorld();
	const float Gravity = (World != nullptr) ? FMath::Abs(World->GetGravityZ()) : 980.0f;
	const float AngleRadians = FMath::DegreesToRadians(LaunchAngleDegrees);

	// 발사각으로 TargetDistance만큼 날아가는 데 필요한 속력.
	// v = sqrt(distance * gravity / sin(2*angle))
	const float SinDoubleAngle = FMath::Max(FMath::Sin(2.0f * AngleRadians), KINDA_SMALL_NUMBER);
	const float Speed = FMath::Sqrt(TargetDistance * Gravity / SinDoubleAngle);

	const FVector LaunchDirection = GetActorForwardVector();
	return LaunchDirection * (Speed * FMath::Cos(AngleRadians)) + FVector::UpVector * (Speed * FMath::Sin(AngleRadians));
}

void AJumpPad::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 발사 결과(속도)는 캐릭터 무브먼트 컴포넌트가 복제하므로, 서버에서만 적용하면 된다.
	if (!HasAuthority())
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	const UWorld* World = GetWorld();
	if (Character == nullptr || World == nullptr)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastLaunchTime < RetriggerCooldown)
	{
		return;
	}
	LastLaunchTime = Now;

	Character->LaunchCharacter(CalculateLaunchVelocity(), true, true);
}
