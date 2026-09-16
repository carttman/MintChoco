#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Weapons/PaintDeposit.h"
#include "Weapons/PaintWeaponProfile.h"

#include "PaintSniperProfile.generated.h"

class APaintVolley;
class UPaintballProfile;

/**
 * A sniper: one hitscan ray per fully charged trigger. The ray stops at the first pawn or world
 * surface it meets, or at Range in open air. Whatever it hit, the ground under the whole ray is
 * painted as a stripe, so a shot fired upwards at 45 degrees still paints the floor beneath its
 * path; a surface at the end of the ray gets an impact stamp of its own. A pawn on the ray is the
 * victim and ends it there.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UPaintSniperProfile : public UPaintWeaponProfile
{
	GENERATED_BODY()

public:
	UPaintSniperProfile();

	virtual bool Fire(const FPaintFireContext& Context, FPaintStrokeState& Stroke, FPaintShot& OutShot) const override;
	virtual void PlayCosmetic(UWorld& World, APawn* Instigator, const FPaintShot& Shot) const override;
	virtual void LogUnsetReferences(const UObject* Owner) const override;

	/** What the ray leaves on the surface it ends on. Nothing when it ends on a pawn or in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper")
	FPaintDeposit Impact;

	/** What each sample of the ray leaves on the ground straight below it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper")
	FPaintDeposit Trail;

	/** How far the ray reaches when nothing stops it, at a full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Range = 10000.0f;

	/**
	 * 충전이 이만큼 모자랄 때마다 사거리가 절반이 된다(초). 0 이면 충전량이 사거리를 바꾸지
	 * 않고, 이 값을 넣지 않은 기존 프로필은 전과 같다.
	 *
	 * 0.5 로 두고 ChargeTime 을 1.5 로 두면 1.0초 충전이 절반, 0.5초 충전이 1/4 이 된다.
	 * 줄어드는 것은 광선의 길이뿐이라 순차 발사의 발수도 따라 줄어든다 - 덜 충전한 샷은
	 * 짧은 줄무늬를 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "s"))
	float RangeHalvingSeconds = 0.0f;

	/**
	 * 만충으로 맞혔을 때의 스턴(초). Impact.StunDuration x 충전비율 대신 이 값을 쓴다. 0 이면
	 * 만충도 다른 충전량과 같은 규칙을 따른다.
	 *
	 * 만충에서만 뛰게 하려는 것이다: Impact.StunDuration 을 ChargeTime 과 같게 두면 부분
	 * 충전은 "충전한 초 = 스턴 초" 가 되고, 만충만 이 값으로 건너뛴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "s"))
	float FullChargeStunSeconds = 0.0f;

	/**
	 * 만충으로 보는 충전량. 1 을 정확히 요구하면 안 된다 - 충전량이 네트워크로 갈 때
	 * 1/255 눈금으로 눌리므로, 한 프레임 차이로 254 가 되면 스턴이 3초에서 절반으로 뚝 떨어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.5", ClampMax = "1"))
	float FullChargeThreshold = 0.99f;

	/** 이 충전량에서 광선이 닿는 거리(cm). 만충이면 Range 그대로. */
	UFUNCTION(BlueprintPure, Category = "Sniper")
	float GetRangeFor(float ChargeFraction) const;

	/**
	 * Distance along the ray between two trail samples. Every sample that finds ground costs a
	 * stamp draw on every machine, so this spacing is what keeps one shot affordable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "10", ForceUnits = "cm"))
	float TrailSpacing = 40.0f;

	/** How far below the ray a sample looks for ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TrailDropHeight = 2000.0f;

	/** Hard cap on trail samples per shot, whatever Range / TrailSpacing says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0"))
	int32 MaxTrailSplats = 256;

	/**
	 * 조준선을 따라 **총구에서** 한 발씩 차례로 나가는 탄. 비워 두면 순차 발사가 없고
	 * 지금까지와 똑같이 즉발 Trail 만 칠한다.
	 *
	 * 디저트 폭격의 APaintRain 이 아니라 APaintVolley 를 쓴다. 폭격은 하늘에서 떨어지지만
	 * 차지샷은 무기다 — 탄은 좌클릭 무기와 같은 총구에서 나가야 한다. 같은 순차 연출이라도
	 * 탄이 태어나는 곳이 다르면 전혀 다른 무기가 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley")
	TObjectPtr<UPaintballProfile> VolleyPaintball;

	/** 비워 두면 APaintVolley 그대로. 연출을 붙이려면 서브클래스 BP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley")
	TSubclassOf<APaintVolley> VolleyClass;

	/** 착탄 지점 사이의 거리(cm). 조준선을 따라 잰다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley", meta = (ClampMin = "10", ForceUnits = "cm"))
	float VolleySpacing = 300.0f;

	/** 한 발과 다음 발 사이의 시간(초). 이 값 × 발수가 연출 전체의 길이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float VolleyInterval = 0.04f;

	/**
	 * 총구를 떠나는 속도(cm/s). 모든 탄이 같은 속도로 조준선을 따라간다.
	 *
	 * 빠를수록 멀리까지 금방 닿지만, 떨어지는 동안 나아가는 거리도 그만큼 길어져 낙하가
	 * 완만해 보인다. VolleyDropLead 로 그만큼 미리 꺾어 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float VolleySpeed = 2500.0f;

	/**
	 * 목표보다 이만큼 앞에서 떨어지기 시작한다(cm). 낙하 중에도 앞으로 나아가므로 미리
	 * 꺾어야 조준한 자리에 떨어진다.
	 *
	 * 알맞은 값은 대략 `속도 × √(2 × 총구 높이 ÷ (980 × 탄의 DropGravityScale))` 이다.
	 * 지형에 따라 낙차가 달라지므로 정확히 맞출 수는 없고, 눈으로 보며 맞추는 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley", meta = (ClampMin = "0", ForceUnits = "cm"))
	float VolleyDropLead = 600.0f;

	/** 발수의 상한. 사거리가 길어도 한 발의 비용이 여기서 멈춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley", meta = (ClampMin = "1"))
	int32 MaxVolleyShots = 64;

	/**
	 * 순차 발사를 쓸 때 즉발 Trail 도포를 건너뛴다. VolleyPaintball 이 비어 있으면 아무 뜻도 없다.
	 *
	 * 끄면 줄무늬가 발사 즉시 다 칠해진 뒤에 탄이 도착하므로 순차 연출이 눈에 보이지 않는다.
	 * 그래서 기본은 켜 둔다 — 줄무늬가 탄이 닿는 순서대로 생긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper|Volley")
	bool bSkipTrailWhenVolleying = true;

	/** A ray has no impact speed of its own, so this fakes one for the brush profile's speed term. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float NominalImpactSpeed = 3000.0f;

private:
	/** Paints the ground under the ray from Muzzle along Direction for Length, skipping the shooter and the victim. */
	void PaintTrail(UWorld& World, const FVector& Muzzle, const FVector& Direction, float Length,
		const APawn* Instigator, const AActor* Victim, uint8 PaintId, int32 Seed) const;

	/** 서버 전용. 총구에서 조준선을 따라 한 발씩 나가는 순차 발사를 얹는다. */
	void SpawnTrailVolley(UWorld& World, const FVector& Muzzle, const FVector& Direction, float Length,
		APawn* Shooter, uint8 PaintId, int32 Seed) const;

	static void DrawTracer(const UWorld& World, const FVector& Start, const FVector& End);
};
