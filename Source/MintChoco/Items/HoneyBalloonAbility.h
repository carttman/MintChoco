#pragma once

#include "CoreMinimal.h"

#include "Items/ItemAimAbility.h"

#include "HoneyBalloonAbility.generated.h"

class AUnit;
class UHoneyBalloonProfile;

/**
 * 꿀풍선. 아이템을 쓰면 던졌을 때의 포물선과 착탄 지점이 보이고, 좌클릭하면 서버가 그
 * 방향으로 투사체 하나를 던진다. 나머지는 투사체의 일이다.
 *
 * 조준 제한 시간은 프로필의 Duration(10초)이다. 그 안에 던지지 않으면 효과가 만료되고
 * 아이템은 그대로 잃는다 — 취소 입력은 없다.
 */
UCLASS()
class MINTCHOCO_API UGA_HoneyBalloon : public UItemAimAbility
{
	GENERATED_BODY()

public:
	UGA_HoneyBalloon();

protected:
	virtual void OnAimBegan(AUnit& Unit, const UItemProfile& Profile) override;
	virtual AActor* SpawnPreview(AUnit& Unit, const UItemProfile& Profile) override;
	virtual void UpdatePreview(AUnit& Unit, AActor& InPreview, float DeltaTime) override;
	virtual void OnAimConfirmed(AUnit& Unit, const UItemProfile& Profile) override;

private:
	/**
	 * 던지는 지점과 속도. 미리보기와 실제 투척이 이 하나를 같이 쓰기 때문에, 보이는 궤적의
	 * 끝과 실제 착탄 지점이 어긋날 여지가 없다.
	 */
	static void ComputeThrow(const AUnit& Unit, const UHoneyBalloonProfile& Honey, FVector& OutOrigin, FVector& OutVelocity);

	/** 지금 조준 상태로 궤적을 다시 그린다. */
	void RefreshArc(AUnit& Unit, AActor& InPreview, const UHoneyBalloonProfile& Honey) const;

	/** 진짜 공의 굵기. 조준 시작에 CDO에서 한 번 읽는다. */
	float BallRadius = 0.0f;
};
