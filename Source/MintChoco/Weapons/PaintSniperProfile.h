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

	/** How far the ray reaches when nothing stops it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Range = 10000.0f;

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
