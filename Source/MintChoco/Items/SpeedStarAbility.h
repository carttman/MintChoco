#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAbility.h"

#include "SpeedStarAbility.generated.h"

class ACharacter;
class AGameGameState;
class USpeedStarProfile;
struct FHitResult;

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
	 * 자국을 찍을 바닥을 찾는다. At은 캡슐 중심이고, 발밑 MarkGroundReach까지 내려본다.
	 *
	 * 공중에 떠 있어도 지나간 자리 아래가 칠해지도록 그 거리만큼 멀리 본다. 사거리 밖이면 거짓이고,
	 * 그때는 자국을 찍지 않는다(끝없이 아래를 칠하지는 않는다).
	 */
	static bool FindTrailGround(const ACharacter& Character, const USpeedStarProfile& Profile, const FVector& At, FHitResult& OutHit);

	/**
	 * 자국 하나를 찍을 위치에서 바닥을 찾아 칠한다. 사거리 안에 바닥이 없으면 아무것도 안 한다.
	 * Direction은 달리는 수평 방향(단위 벡터): 자국이 그 반대쪽(발 뒤)으로 Stretch 배만큼 끌린다. 1이면 둥근 자국.
	 */
	static bool DropMark(AUnit& Unit, const USpeedStarProfile& Profile, const FVector& At, const FVector& Direction, float Stretch, uint8 PaintId, uint8 StarGen);

	/**
	 * 코너를 돈 뒤 StraightRun만큼 곧게 달렸을 때 이 자국에 허용되는 배율. 꼬리(반지름 × 배율 + 중심 이동)가
	 * 직진 구간 안에 머물도록 잡아, 방향이 확 꺾여도 자국이 원래 자국 옆으로 삐져나오지 않는다.
	 * 이전 자국의 반지름만큼은 늘 허용되므로 코너 자국은 둥글고, 그 뒤로 TrailStretch까지 차오른다.
	 */
	static float StretchForRun(const USpeedStarProfile& Profile, float StraightRun);

protected:
	virtual void OnItemActivated(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleTick(float DeltaTime);

	/** 직진 거리를 갱신하고 그에 맞는 배율로 자국을 찍는다. */
	void PlaceMark(AUnit& Unit, const FVector& At, const FVector& Direction);

	UPROPERTY(Transient)
	TObjectPtr<const USpeedStarProfile> Star;

	/** 마지막 자국의 위치. 다음 자국은 여기서 MarkSpacing만큼 간 곳이다. */
	FVector LastMark = FVector::ZeroVector;

	/** 마지막 자국의 진행 방향과, 그 방향으로 곧게 달린 거리(cm). 꺾이면 각도만큼 줄어 자국이 다시 둥글어진다. */
	FVector LastDirection = FVector::ZeroVector;
	float StraightRun = 0.0f;

	/** 서버 전용. 시작을 알린 게임 스테이트와, 자국이 싣는 값들. 끝은 EndAbility가 알린다. */
	TWeakObjectPtr<AGameGameState> StarState;
	uint8 StarPaintId = 0;
	uint8 StarGen = 0;
	bool bStarPaintBegun = false;
};
