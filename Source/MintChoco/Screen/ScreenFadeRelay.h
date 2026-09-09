#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ScreenFadeRelay.generated.h"

/**
 * 서버가 모든 클라이언트에게 "지금 어두워져라"를 알리는 복제 액터.
 *
 * 서버 트래블 전에 모두가 페이드 아웃해야 하는데, 그 신호를 실을 공통 클래스가 없다.
 * 플레이어 컨트롤러는 맵마다 다른 블루프린트고 GameState도 맵마다 다르다. 항상 관련
 * 있는 액터 하나를 필요할 때 스폰해 멀티캐스트 한 번 보내는 쪽이 계층을 건드리지 않는다.
 */
UCLASS(NotPlaceable)
class MINTCHOCO_API AScreenFadeRelay : public AActor
{
	GENERATED_BODY()

public:
	AScreenFadeRelay();

	/** 서버 포함 모든 머신의 가림막 서브시스템에 페이드 아웃을 건다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFadeOut(float Duration);
};
