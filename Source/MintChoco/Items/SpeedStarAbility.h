#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SpeedStarAbility.generated.h"

class AGameGameState;
class USpeedStarProfile;

/**
 * 스피드 스타. 속도 자체는 이 클래스가 만지지 않는다. GE가 붙인 상태 태그를 슬롯
 * 컴포넌트가 보고 무브먼트의 부스트 플래그를 세우며, 그 플래그가 대시처럼 압축 플래그로
 * 서버에 간다. 여기서는 서버가 지나간 길에 자국만 남긴다.
 *
 * 자국은 보통 팀 페인트지만 스타 세대(FPaintSplat::StarGen)를 달고 간다. 효과 중에는 그
 * 세대의 자국이 무지개로 빛나고 다른 id가 덮지 못하며, 끝나면 팀 색으로 바랜다. 세대와
 * 시각은 게임 스테이트가 복제한다(AGameGameState::BeginStarPaint / EndStarPaint).
 */
UCLASS()
class MINTCHOCO_API UGA_SpeedStar : public UItemAbility
{
	GENERATED_BODY()

public:
	UGA_SpeedStar();

	/**
	 * 자국 하나를 찍을 위치에서 바닥을 찾아 칠한다. 바닥이 없으면(공중) 아무것도 안 한다.
	 * Direction은 달리는 수평 방향(단위 벡터): 자국이 그쪽으로 TrailStretch만큼 번진다. 0이면 둥근 자국.
	 */
	static bool DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At, const FVector& Direction, uint8 PaintId, uint8 StarGen);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<const USpeedStarProfile> Star;

	/** 마지막 자국의 위치. 다음 자국은 여기서 MarkSpacing만큼 간 곳이다. */
	FVector LastMark = FVector::ZeroVector;

	/** 서버 전용. 시작을 알린 게임 스테이트와, 자국이 싣는 값들. 끝은 EndAbility가 알린다. */
	TWeakObjectPtr<AGameGameState> StarState;
	uint8 StarPaintId = 0;
	uint8 StarGen = 0;
	bool bStarPaintBegun = false;
};
