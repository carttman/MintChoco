#include "Items/LandingMarker.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	/** 표시일 뿐이라 아무것도 막지 않고 그림자도 만들지 않는다. */
	void MakeCosmetic(UStaticMeshComponent& Ring)
	{
		Ring.SetMobility(EComponentMobility::Movable);
		Ring.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring.SetGenerateOverlapEvents(false);
		Ring.SetCastShadow(false);
	}
}

ALandingMarker::ALandingMarker()
{
	// 어빌리티가 매 프레임 위치와 충전량을 밀어 넣는다. 스스로 틱할 것이 없다.
	PrimaryActorTick.bCanEverTick = false;
	// 조준하는 사람 화면에만 있는 표시다. 복제하면 남의 화면에 내 조준이 보인다.
	bReplicates = false;

	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	MaxRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MaxRing"));
	MaxRing->SetupAttachment(Origin);
	MakeCosmetic(*MaxRing);

	ChargeRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeRing"));
	ChargeRing->SetupAttachment(Origin);
	MakeCosmetic(*ChargeRing);
}

void ALandingMarker::BeginPlay()
{
	Super::BeginPlay();

	// BP가 지정한 메시와 재질을 양쪽에 같이 물린다. 컴포넌트마다 따로 고르게 두면 둘이
	// 어긋난 채로 저장될 수 있다 — 같은 그림의 크고 작은 두 장이라는 것이 이 표시의 전부다.
	for (UStaticMeshComponent* const Ring : { MaxRing.Get(), ChargeRing.Get() })
	{
		if (!Ring)
		{
			continue;
		}
		if (RingMesh)
		{
			Ring->SetStaticMesh(RingMesh);
		}
		if (RingMaterial)
		{
			Ring->SetMaterial(0, RingMaterial);
		}
	}

	RefreshRings();
}

void ALandingMarker::SetMaxRadius(float NewMaxRadius)
{
	MaxRadius = FMath::Max(NewMaxRadius, 1.0f);
	RefreshRings();
}

void ALandingMarker::SetCharge(float NewCharge)
{
	const float Clamped = FMath::Clamp(NewCharge, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Charge, Clamped))
	{
		return;
	}
	Charge = Clamped;
	RefreshRings();
}

void ALandingMarker::RefreshRings()
{
	const float ThicknessScale = RingThickness / ShapeDiameter;

	if (MaxRing)
	{
		// 지름이 반지름의 두 배다. 기본 원기둥이 지름 100 cm라 반지름 R은 스케일 2R/100.
		const float MaxScale = 2.0f * MaxRadius / ShapeDiameter;
		MaxRing->SetRelativeScale3D(FVector(MaxScale, MaxScale, ThicknessScale));
	}

	if (ChargeRing)
	{
		// 0에서도 MinRadiusFraction만큼은 있다. 어빌리티가 효과에 쓰는 배율과 같은 식이라
		// 보이는 원이 곧 실제 반경이다.
		const float Fraction = FMath::Lerp(MinRadiusFraction, 1.0f, Charge);
		const float ChargeScale = 2.0f * MaxRadius * Fraction / ShapeDiameter;
		ChargeRing->SetRelativeScale3D(FVector(ChargeScale, ChargeScale, ThicknessScale));

		// 두 원을 같은 높이에 두면 서로 번갈아 이겨 깜빡인다. 상대 위치라 액터가 움직여도
		// 따라간다.
		ChargeRing->SetRelativeLocation(FVector(0.0f, 0.0f, ChargeRingLift));
	}
}
