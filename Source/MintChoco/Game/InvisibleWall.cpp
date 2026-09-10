#include "Game/InvisibleWall.h"

#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	/** 기본 크기: 폭 100, 두께 20, 높이 300 cm. 박스 확장 크기는 절반값이다. */
	const FVector DefaultHalfExtent(50.0f, 10.0f, 150.0f);

	/** 엔진 BasicShapes/Cube는 한 변 100 cm. 미리보기 메시를 박스 크기에 맞추는 배율. */
	constexpr float BasicCubeSize = 100.0f;
}

AInvisibleWall::AInvisibleWall()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// 폰만 막는다. 카메라, 시야·조준 트레이스, 페인트탄(Paintball 채널), 아이템 투사체(WorldDynamic)는
	// 전부 Ignore라 벽이 없는 것처럼 지나간다.
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(DefaultHalfExtent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);

#if WITH_EDITORONLY_DATA
	// 에디터 전용 컴포넌트: 게임 월드에는 만들어지지 않는다. 혹시 남더라도 숨긴다.
	Preview = CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(TEXT("Preview"));
	if (Preview)
	{
		Preview->SetupAttachment(Box);
		Preview->SetRelativeScale3D(DefaultHalfExtent * 2.0f / BasicCubeSize);
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Preview->SetGenerateOverlapEvents(false);
		Preview->SetCastShadow(false);
		Preview->SetHiddenInGame(true);
	}

	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Box);
	}
#endif
}
