// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JumpPad_Arc.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 밟으면 정해진 각도와 목표 거리로 캐릭터를 날려 보내는 점프대.
 *
 * 런타임에 스폰되지 않는다. 레벨에 직접 배치(또는 이 클래스를 부모로 한 블루프린트를
 * 배치)해서 쓰는 액터다. 발사 속도는 BeginPlay가 아니라 밟는 순간마다
 * TargetDistance/LaunchAngleDegrees로부터 다시 계산되므로, 배치 후 액터를 옮기거나
 * 회전시켜도 그 방향으로 그대로 날아간다.
 *
 * 서버만 실제로 LaunchCharacter를 호출한다. 그 결과 속도는 캐릭터의 무브먼트
 * 컴포넌트가 알아서 복제하므로, 이 액터 자체는 복제할 것이 없다.
 */
UCLASS(Blueprintable)
class MINTCHOCO_API AJumpPad_Arc : public AActor
{
	GENERATED_BODY()

public:
	AJumpPad_Arc();

protected:
	virtual void BeginPlay() override;

	/** 캐릭터가 밟았는지 감지하는 트리거. 루트 컴포넌트이며 이것만 충돌을 갖는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** 시각적으로만 쓰는 메시. 충돌 판정은 TriggerBox가 전담한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/**
	 * 이 점프대가 캐릭터를 보내는 수평 거리(cm). 액터가 바라보는 방향(전방 벡터)으로
	 * 날아가므로, 배치할 때 회전으로 발사 방향을 정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad", meta = (ClampMin = "100.0"))
	float TargetDistance = 2000.0f;

	/** 발사 각도(도, 지면 기준). 값이 작을수록 멀리 낮게, 클수록 위로 솟구친다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad", meta = (ClampMin = "5.0", ClampMax = "85.0"))
	float LaunchAngleDegrees = 55.0f;

	/** 한 번 발사한 뒤 다시 발사 가능해지기까지의 시간(초). 착지 직후 재발사를 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump Pad", meta = (ClampMin = "0.0"))
	float RetriggerCooldown = 1.0f;

	/**
	 * 실제로 캐릭터에 적용할 발사 속도를 계산한다.
	 * 다른 궤적(예: 수직으로만 튕기기, 고정 속도로 날리기)이 필요한 점프대는
	 * 이 함수만 오버라이드한 서브클래스로 확장하면 된다.
	 */
	virtual FVector CalculateLaunchVelocity() const;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	/** 재발사 쿨다운 판정용. 아직 한 번도 발사하지 않았을 때도 첫 발사를 막지 않도록 충분히 작은 값으로 시작한다. */
	double LastLaunchTime = -1000.0;
};
