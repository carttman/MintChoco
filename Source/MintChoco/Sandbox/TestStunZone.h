#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestStunZone.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * 테스트용 스턴 큐브. 맵에 놓아 두고 범위 안에 들어온 캐릭터를 잠깐 세운다.
 *
 * **버리기 위해 만든 액터다.** 이 파일 둘과 레벨에 놓인 인스턴스만 지우면 흔적이 남지 않는다.
 * 그렇게 되도록 일부러 아무것도 건드리지 않고 만들었다 —
 *
 * - 기존 파일을 한 줄도 고치지 않는다. `AUnit::TryApplyStun`은 이미 공개된 함수다.
 * - 새 게임플레이 태그도, 열거형 값도, 설정 항목도 만들지 않는다.
 * - 공용 데이터 에셋을 참조하지 않는다. 외형은 엔진 기본 큐브를 그대로 쓴다.
 *
 * 스턴과 슈퍼아머는 `AUnit::TryApplyStun(StunSeconds, SuperArmorSeconds)` 하나로 건다. 그래서
 * 아이템 스턴과 **완전히 같은 규칙**을 탄다: 이미 스턴 중이거나 슈퍼아머면 걸리지 않고,
 * 스턴이 끝나는 순간 슈퍼아머가 이어진다.
 *
 * 복제하지 않는다. 레벨에 놓인 액터라 모든 머신에 이미 있고, 스턴을 거는 것은 서버의 일이므로
 * 서버의 오버랩만 쓴다(`TryApplyStun`이 권한을 직접 확인한다). 점프대와 같은 방식이다.
 */
UCLASS()
class MINTCHOCO_API ATestStunZone : public AActor
{
	GENERATED_BODY()

public:
	ATestStunZone();

#if WITH_EDITOR
	/** 에디터에서 반경을 바꾸면 구체도 같이 커지도록. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;

	/** 발동 범위(cm). 캐릭터의 캡슐이 이 구체에 닿는 순간 한 번 걸린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TestStunZone", meta = (ClampMin = "1", ForceUnits = "cm"))
	float TriggerRadius = 400.0f;

	/** 스턴 길이(초). 0 이하면 아무 일도 하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TestStunZone", meta = (ClampMin = "0", ForceUnits = "s"))
	float StunSeconds = 1.0f;

	/**
	 * 스턴이 **끝난 뒤** 이어지는 슈퍼아머 길이(초). 스턴과 겹치지 않는다.
	 * 그동안은 이 큐브에 다시 걸리지 않으므로 사실상 재발동 쿨다운 노릇도 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TestStunZone", meta = (ClampMin = "0", ForceUnits = "s"))
	float SuperArmorSeconds = 5.0f;

	/** 발동 범위. 크기는 TriggerRadius가 정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TestStunZone")
	TObjectPtr<USphereComponent> Trigger;

	/** 눈에 보이는 큐브. 로직도 충돌도 없다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TestStunZone")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
