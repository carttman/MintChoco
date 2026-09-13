#include "Items/AimArcPreview.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"

AAimArcPreview::AAimArcPreview()
{
	// 궤적은 어빌리티가 매 프레임 SetArc로 밀어 넣는다. 스스로 틱할 것이 없다.
	PrimaryActorTick.bCanEverTick = false;
	// 조준하는 사람 화면에만 있는 표시다. 복제하면 남의 화면에 내 조준이 보인다.
	bReplicates = false;

	Dots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Dots"));
	SetRootComponent(Dots);
	Dots->SetMobility(EComponentMobility::Movable);
	Dots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dots->SetGenerateOverlapEvents(false);
	Dots->SetCastShadow(false);

	Impact = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));
	Impact->SetupAttachment(Dots);
	Impact->SetMobility(EComponentMobility::Movable);
	Impact->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Impact->SetGenerateOverlapEvents(false);
	Impact->SetCastShadow(false);
	// 첫 SetArc가 착탄 지점을 정해 줄 때까지는 보이지 않는다.
	Impact->SetHiddenInGame(true);
}

void AAimArcPreview::SetArc(const TArray<FVector>& Path, bool bHasImpact, const FVector& ImpactPoint, const FVector& ImpactNormal)
{
	DotTransforms.Reset();

	const FVector DotScale(DotSize / ShapeSize);
	const float Spacing = FMath::Max(DotSpacing, 1.0f);

	// 경로 전체를 하나의 자로 재고 Spacing마다 점을 찍는다. 구간마다 0에서 다시 세면
	// 예측 점이 촘촘한 곳(궤적이 꺾이는 꼭대기)마다 점이 뭉친다.
	float Travelled = 0.0f;
	float NextAt = 0.0f;
	for (int32 Index = 0; Index + 1 < Path.Num() && DotTransforms.Num() < MaxDots; ++Index)
	{
		const FVector& From = Path[Index];
		const FVector& To = Path[Index + 1];
		const float Length = FVector::Dist(From, To);
		if (Length <= UE_SMALL_NUMBER)
		{
			continue;
		}

		while (NextAt <= Travelled + Length && DotTransforms.Num() < MaxDots)
		{
			const float Alpha = (NextAt - Travelled) / Length;
			DotTransforms.Emplace(FRotator::ZeroRotator, FMath::Lerp(From, To, Alpha), DotScale);
			NextAt += Spacing;
		}
		Travelled += Length;
	}

	// 개수가 그대로면 자리만 고쳐 쓴다. 매 프레임 지우고 다시 넣으면 렌더 상태가 통째로 다시 만들어진다.
	if (Dots->GetInstanceCount() == DotTransforms.Num())
	{
		Dots->BatchUpdateInstancesTransforms(0, DotTransforms, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	}
	else
	{
		Dots->ClearInstances();
		Dots->AddInstances(DotTransforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true, /*bUpdateNavigation=*/false);
	}

	Impact->SetHiddenInGame(!bHasImpact);
	if (bHasImpact)
	{
		// 맞은 면을 따라 눕힌다. 바닥이면 바닥에, 벽이면 벽에 붙은 판으로 보인다.
		const FRotator Facing = FRotationMatrix::MakeFromZ(ImpactNormal).Rotator();
		const FVector Scale(ImpactSize / ShapeSize, ImpactSize / ShapeSize, ImpactThickness / ShapeSize);
		// 면에서 살짝 띄우지 않으면 z-파이팅으로 얼룩진다.
		const FVector Centre = ImpactPoint + ImpactNormal * (ImpactThickness * 0.5f + 1.0f);
		Impact->SetWorldTransform(FTransform(Facing, Centre, Scale));
	}
}
