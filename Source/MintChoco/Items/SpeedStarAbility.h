#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SpeedStarAbility.generated.h"

class USpeedStarProfile;

/**
 * 스피드 스타. 속도 자체는 이 클래스가 만지지 않는다. GE가 붙인 상태 태그를 슬롯
 * 컴포넌트가 보고 무브먼트의 부스트 플래그를 세우며, 그 플래그가 대시처럼 압축 플래그로
 * 서버에 간다. 여기서는 서버가 지나간 길에 자국만 남긴다.
 */
UCLASS()
class MINTCHOCO_API UGA_SpeedStar : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_SpeedStar();

	/** 자국 하나를 찍을 위치에서 바닥을 찾아 칠한다. 바닥이 없으면(공중) 아무것도 안 한다. */
	static bool DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<const USpeedStarProfile> Star;

	/** 마지막 자국의 위치. 다음 자국은 여기서 MarkSpacing만큼 간 곳이다. */
	FVector LastMark = FVector::ZeroVector;
};
