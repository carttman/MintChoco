#include "Sandbox/TestStunZone.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/Unit.h"

ATestStunZone::ATestStunZone()
{
	PrimaryActorTick.bCanEverTick = false;
	// 레벨에 놓인 액터라 모든 머신에 이미 있다. 스턴은 서버가 걸고 GAS가 복제하므로
	// 이 액터 자체는 복제할 것이 없다(점프대와 같은 이유).
	bReplicates = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	// 엔진 기본 큐브를 그대로 쓴다. /Game에 에셋을 하나도 만들지 않아야 지울 때 깔끔하다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ATestStunZone::BeginPlay()
{
	Super::BeginPlay();

	// 반경은 에디터에서 바꿀 수 있으므로 시작할 때 한 번 맞춘다.
	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATestStunZone::OnTriggerBeginOverlap);
}

#if WITH_EDITOR
void ATestStunZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Trigger)
	{
		Trigger->SetSphereRadius(TriggerRadius);
	}
}
#endif

void ATestStunZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 스턴은 서버의 일이다. 클라이언트에서도 오버랩은 울리지만 TryApplyStun이 스스로 거른다.
	if (!HasAuthority())
	{
		return;
	}

	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (!Unit)
	{
		return;
	}

	// 들어오는 순간 한 번만. 범위 안에 머무르는 동안은 다시 걸지 않는다 — 나갔다 들어와야 한다.
	// 이미 스턴 중이거나 슈퍼아머면 TryApplyStun이 거절하므로 여기서 따로 볼 것이 없다.
	Unit->TryApplyStun(StunSeconds, SuperArmorSeconds);
}
