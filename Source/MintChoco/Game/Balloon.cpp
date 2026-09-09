#include "Game/Balloon.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "MintChoco.h"
#include "Weapons/PaintProjectile.h"
#include "Weapons/PaintballProfile.h"

// ---------------------------------------------------------------- FBalloonState

bool FBalloonState::Hit(float HitPower, uint8 PaintId, float MaxHealth)
{
	if (Damage >= MaxHealth || HitPower <= 0.0f)
	{
		return false;
	}

	if (Teams::IsValidId(static_cast<int32>(PaintId)))
	{
		LastTeam = static_cast<int32>(PaintId);
	}
	Damage += HitPower;
	return Damage >= MaxHealth;
}

void FBalloonState::Reset()
{
	Damage = 0.0f;
	LastTeam = Teams::None;
}

// ---------------------------------------------------------------- ABalloon

ABalloon::ABalloon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	// 페인트탄만 막는다. 플레이어는 지나가고 카메라도 밀리지 않는다. 스나이퍼 광선도
	// Paintball 채널로 트레이스하므로 여기에 걸린다.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(60.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(PaintballChannel, ECR_Block);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
}

void ABalloon::BeginPlay()
{
	Super::BeginPlay();
	BaseScale = Mesh ? Mesh->GetRelativeScale3D() : FVector::OneVector;
	ApplyLook();
}

void ABalloon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABalloon, State);
	DOREPLIFETIME(ABalloon, bPopped);
}

void ABalloon::ReceivePaintHit_Implementation(float HitPower, uint8 PaintId, const FHitResult& Hit)
{
	// 타격은 서버의 ApplyHit에서만 온다. 클라이언트는 복제로 따라온다.
	if (!HasAuthority() || bPopped)
	{
		return;
	}

	const bool bBurst = State.Hit(HitPower, PaintId, MaxHealth);
	ApplyLook();
	BP_OnHit(State.GetFraction(MaxHealth), State.LastTeam);

	if (bBurst)
	{
		Pop(State.LastTeam);
	}
}

void ABalloon::Pop(int32 PoppingTeam)
{
	bPopped = true;
	ApplyLook();
	BP_OnPopped(PoppingTeam);

	// 팀이 없는 타격만으로 터졌다면(예약 id) 뿌릴 색이 없다. 터지기만 한다.
	if (Teams::IsValidId(PoppingTeam) && BurstPaintball)
	{
		MulticastBurst(FMath::Rand(), static_cast<uint8>(PoppingTeam));
	}

	UE_LOG(LogMintChoco, Log, TEXT("%s: %s 팀이 풍선을 터뜨렸다."), *GetNameSafe(this), Teams::GetDisplayName(PoppingTeam));

	if (RespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ABalloon::Inflate, RespawnDelay, /*bLoop=*/false);
	}
}

void ABalloon::Inflate()
{
	State.Reset();
	bPopped = false;
	ApplyLook();
	BP_OnInflated();
}

void ABalloon::ComputeBurstDirections(int32 Seed, int32 Count, TArray<FVector>& OutDirections)
{
	OutDirections.Reset(Count);
	const FRandomStream Random(Seed);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// 방위각은 고르게 돌리고, 고도는 수평 위 10~70도 사이. 대부분 바닥에 떨어지고
		// 일부는 벽에 닿는다.
		const float Yaw = (static_cast<float>(Index) + Random.FRand()) * (360.0f / FMath::Max(Count, 1));
		const float Pitch = Random.FRandRange(10.0f, 70.0f);
		OutDirections.Add(FRotator(Pitch, Yaw, 0.0f).Vector());
	}
}

void ABalloon::MulticastBurst_Implementation(int32 Seed, uint8 PaintId)
{
	UWorld* const World = GetWorld();
	if (!World || !BurstPaintball)
	{
		return;
	}

	// 서버의 탄이 칠하고, 클라이언트의 탄은 같은 궤적의 그림이다.
	const bool bCosmetic = !HasAuthority();

	TArray<FVector> Directions;
	ComputeBurstDirections(Seed, BurstCount, Directions);

	const FVector Origin = GetActorLocation();
	for (int32 Index = 0; Index < Directions.Num(); ++Index)
	{
		const int32 BallSeed = static_cast<int32>(HashCombineFast(static_cast<uint32>(Seed), static_cast<uint32>(Index)));
		const FTransform SpawnTransform(Directions[Index].Rotation(), Origin);
		BurstPaintball->Launch(*World, SpawnTransform, /*Instigator=*/nullptr, Directions[Index] * BurstSpeed, PaintId, BallSeed, bCosmetic);
	}
}

void ABalloon::OnRep_State()
{
	ApplyLook();
	BP_OnHit(State.GetFraction(MaxHealth), State.LastTeam);
}

void ABalloon::OnRep_Popped()
{
	ApplyLook();
	if (bPopped)
	{
		BP_OnPopped(State.LastTeam);
	}
	else
	{
		BP_OnInflated();
	}
}

void ABalloon::ApplyLook()
{
	if (Collision)
	{
		Collision->SetCollisionEnabled(bPopped ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
	if (!Mesh)
	{
		return;
	}

	Mesh->SetVisibility(!bPopped);
	Mesh->SetRelativeScale3D(BaseScale * FMath::Lerp(1.0f, InflateScale, State.GetFraction(MaxHealth)));

	if (Teams::IsValidId(State.LastTeam) && !ColorParameterName.IsNone())
	{
		if (UMaterialInstanceDynamic* const Dynamic = GetOrCreateMaterial())
		{
			Dynamic->SetVectorParameterValue(ColorParameterName, FLinearColor(Teams::GetDisplayColor(State.LastTeam)));
		}
	}
	else if (Material)
	{
		// 다시 부풀어 오르면 팀이 없다. 원래 색(흰 배율)으로 되돌린다.
		Material->SetVectorParameterValue(ColorParameterName, FLinearColor::White);
	}
}

UMaterialInstanceDynamic* ABalloon::GetOrCreateMaterial()
{
	if (!Material && Mesh && Mesh->GetMaterial(0))
	{
		Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	return Material;
}
