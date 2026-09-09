#include "Items/ChocolateFountain.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

#include "Game/Unit.h"
#include "MintChoco.h"
#include "Weapons/PaintProjectile.h"

namespace
{
	/** 엔진 BasicShapes/Sphere는 지름 100 cm. */
	constexpr float BasicSphereRadius = 50.0f;

	/** Sensor는 Wall보다 이만큼 크다. 캡슐이 Wall을 완전히 벗어나야 통과 목록에서 빠진다. */
	constexpr float SensorMargin = 40.0f;
}

AChocolateFountain::AChocolateFountain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	// Wall: 폰은 막고 페인트탄은 겹친다(공 쪽이 Block이어도 min을 취해 Overlap). 카메라와 시야
	// 트레이스는 지나간다.
	Wall = CreateDefaultSubobject<USphereComponent>(TEXT("Wall"));
	SetRootComponent(Wall);
	Wall->InitSphereRadius(300.0f);
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Wall->SetCollisionObjectType(ECC_WorldDynamic);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Wall->SetCollisionResponseToChannel(PaintballChannel, ECR_Overlap);
	Wall->SetGenerateOverlapEvents(true);
	Wall->OnComponentBeginOverlap.AddDynamic(this, &AChocolateFountain::OnWallBeginOverlap);

	// Sensor: 폰이 안에 있는지만 본다. Wall을 무시하는 캡슐도 다른 컴포넌트인 이것과는 겹친다.
	Sensor = CreateDefaultSubobject<USphereComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(Wall);
	Sensor->InitSphereRadius(300.0f + SensorMargin);
	Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sensor->SetCollisionObjectType(ECC_WorldDynamic);
	Sensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sensor->SetGenerateOverlapEvents(true);
	Sensor->OnComponentBeginOverlap.AddDynamic(this, &AChocolateFountain::OnSensorBeginOverlap);
	Sensor->OnComponentEndOverlap.AddDynamic(this, &AChocolateFountain::OnSensorEndOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Wall);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
}

void AChocolateFountain::Init(int32 InTeam, float InRadius, float InLifetime)
{
	Team = InTeam;
	Radius = InRadius;
	Lifetime = InLifetime;
}

void AChocolateFountain::ApplyShape()
{
	Wall->SetSphereRadius(Radius);
	Sensor->SetSphereRadius(Radius + SensorMargin);
	Mesh->SetRelativeScale3D(FVector(Radius / BasicSphereRadius));
}

void AChocolateFountain::BeginPlay()
{
	// 초기 복제 속성은 여기 오기 전에 도착해 있다. 모양을 먼저 맞춰야 초기 오버랩이 맞는 반경으로 잡힌다.
	ApplyShape();

	Super::BeginPlay();

	SetLifeSpan(Lifetime);

	// 아군은 어디서나 벽을 통과한다. 리슨 호스트의 폰도 이 머신의 캡슐이므로 여기서 건다.
	UWorld* const World = GetWorld();
	for (TActorIterator<AUnit> It(World); It; ++It)
	{
		if (*It && Teams::IsValidId(Team) && It->GetTeam() == Team)
		{
			SetIgnoresWall(*It, true);
		}
	}

	if (HasAuthority())
	{
		// 초기 오버랩 이벤트는 Super::BeginPlay 안에서 이미 왔다. 놓친 것이 있으면 여기서 줍는다.
		TArray<AActor*> Inside;
		Sensor->GetOverlappingActors(Inside, AUnit::StaticClass());
		for (AActor* const Actor : Inside)
		{
			if (AUnit* const Unit = Cast<AUnit>(Actor))
			{
				AdmitTrappedOpponent(*Unit);
			}
		}
		bSpawnOverlapsDone = true;
	}
	ApplyPassThrough();

	BP_OnRaised(Team);
	UE_LOG(LogMintChoco, Verbose, TEXT("%s: %s 팀의 초콜릿 분수, 반경 %.0f, %d명 갇힘."), *GetNameSafe(this), Teams::GetDisplayName(Team), Radius, PassThrough.Num());
}

void AChocolateFountain::EndPlay(const EEndPlayReason::Type Reason)
{
	for (int32 Index = IgnoringUnits.Num() - 1; Index >= 0; --Index)
	{
		SetIgnoresWall(IgnoringUnits[Index], false);
	}
	Super::EndPlay(Reason);
}

void AChocolateFountain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AChocolateFountain, Team, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AChocolateFountain, Radius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AChocolateFountain, Lifetime, COND_InitialOnly);
	DOREPLIFETIME(AChocolateFountain, PassThrough);
}

void AChocolateFountain::SetIgnoresWall(AUnit* Unit, bool bIgnore)
{
	UPrimitiveComponent* const Capsule = Unit ? Unit->GetCapsuleComponent() : nullptr;
	if (!Capsule || !Wall)
	{
		return;
	}
	Capsule->IgnoreComponentWhenMoving(Wall, bIgnore);
	if (bIgnore)
	{
		IgnoringUnits.AddUnique(Unit);
	}
	else
	{
		IgnoringUnits.Remove(Unit);
	}
}

void AChocolateFountain::ApplyPassThrough()
{
	// 통과 목록에 있는 상대는 무시, 빠진 상대는 다시 막는다. 아군은 팀으로 구분해 그대로 둔다.
	for (AUnit* const Unit : PassThrough)
	{
		if (Unit)
		{
			SetIgnoresWall(Unit, true);
		}
	}
	for (int32 Index = IgnoringUnits.Num() - 1; Index >= 0; --Index)
	{
		AUnit* const Unit = IgnoringUnits[Index];
		const bool bFriendly = Unit && Teams::IsValidId(Team) && Unit->GetTeam() == Team;
		if (Unit && !bFriendly && !PassThrough.Contains(Unit))
		{
			SetIgnoresWall(Unit, false);
		}
	}
}

void AChocolateFountain::OnRep_PassThrough()
{
	ApplyPassThrough();
}

void AChocolateFountain::AdmitTrappedOpponent(AUnit& Unit)
{
	const bool bFriendly = Teams::IsValidId(Team) && Unit.GetTeam() == Team;
	if (bFriendly || PassThrough.Contains(&Unit))
	{
		return;
	}
	PassThrough.Add(&Unit);
	// 중심에서 바깥으로 한 번 밀린다. 슈퍼아머면 밀리지 않고 걸어 나가야 한다.
	Unit.Knockback(GetActorLocation());
}

void AChocolateFountain::OnWallBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	// 상대 탄은 벽에 삼켜진다. 서버의 진짜 탄도, 클라이언트의 연출 탄도 같은 규칙이라 그림이 맞는다.
	APaintProjectile* const Ball = Cast<APaintProjectile>(OtherActor);
	if (Ball && Teams::IsValidId(Team) && Ball->GetPaintId() != static_cast<uint8>(Team))
	{
		Ball->Destroy();
	}
}

void AChocolateFountain::OnSensorBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
	// 생성 순간의 초기 오버랩만 통과 목록에 넣는다. 그 뒤에 들어오려는 상대는 벽이 막는다.
	if (!HasAuthority() || bSpawnOverlapsDone)
	{
		return;
	}
	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (Unit && OtherComp == Unit->GetCapsuleComponent())
	{
		AdmitTrappedOpponent(*Unit);
	}
}

void AChocolateFountain::OnSensorEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32)
{
	if (!HasAuthority())
	{
		return;
	}
	AUnit* const Unit = Cast<AUnit>(OtherActor);
	if (Unit && OtherComp == Unit->GetCapsuleComponent() && PassThrough.Remove(Unit) > 0)
	{
		// 서버(리슨 호스트)는 RepNotify가 오지 않으므로 직접 반영한다.
		ApplyPassThrough();
	}
}
