// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumpPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 맵에 배치하는 점프대. 캐릭터의 캡슐이 트리거에 닿는 순간 위로 띄운다.
 *
 * 상승 속도는 덮어쓰고 수평 속도는 건드리지 않는다. 그래서 뛰어오르는 높이는 누가
 * 어떻게 밟든 항상 같고, 달려온 기세는 그대로 살아나 그만큼 멀리 날아간다.
 *
 * 복제하지 않는다. 레벨에 배치된 액터라 모든 머신에 이미 존재하고 충돌도 각자
 * 로컬에서 돌기 때문에, 소유 클라이언트와 서버가 같은 순간에 같은 계산으로 발사하면
 * 결과가 어긋나지 않는다. 서버만 발사시키고 결과를 복제하면 소유 클라이언트는
 * RTT만큼 늦게 튀어오른 뒤 ClientAdjustPosition으로 끌려간다. UUnitMovementComponent의
 * 대시가 속도가 아니라 의도를 압축 플래그로 보내는 것과 같은 이유다.
 */
UCLASS()
class MINTCHOCO_API AJumpPad : public AActor
{
	GENERATED_BODY()

public:
	AJumpPad();

protected:
	/**
	 * 발사할 상승 속도(cm/s). Z를 덮어쓰므로 이 값 하나가 도약 높이를 정한다.
	 *
	 * 밟기 직전에 떨어지고 있었든 올라가고 있었든 결과가 같아야 하므로 더하지 않고
	 * 덮어쓴다. 더하면 낙하 중에 밟았을 때 거의 튀지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JumpPad", meta = (ClampMin = "0.0"))
	float LaunchZSpeed = 1200.0f;

	/** 발동 범위. 캐릭터의 캡슐이 여기에 겹치면 발사한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPad")
	TObjectPtr<UBoxComponent> Trigger;

	/** 외형. 로직도 충돌도 없다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPad")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
