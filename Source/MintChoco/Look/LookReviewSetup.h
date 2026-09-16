#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "LookReviewSetup.generated.h"

class UAnimationAsset;
class UPaintBrushProfile;
class USkeletalMesh;

/** 리뷰 시작 때 찍는 페인트 한 방. Start 에서 End 로 PaintballChannel 을 쏴서 맞은 자리에 찍는다. */
USTRUCT(BlueprintType)
struct FLookReviewSplat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Review")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Review")
	FVector End = FVector::ZeroVector;

	/** 팀 id 와 같다. 0 민트, 1 초코. */
	UPROPERTY(EditAnywhere, Category = "Review", meta = (ClampMin = "0", ClampMax = "3"))
	int32 PaintId = 0;

	/** UPaintBrushProfile::BuildSplat 의 Volume. 반지름이 제곱근으로 커진다. */
	UPROPERTY(EditAnywhere, Category = "Review", meta = (ClampMin = "0"))
	float Volume = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Review", meta = (ClampMin = "0", ClampMax = "1"))
	float HeightAdd = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Review")
	int32 Seed = 0;
};

/** 리뷰 동안 세워 두는 캐릭터. 컨트롤러도 게임플레이도 없는 스켈레탈 메시 액터다. */
USTRUCT(BlueprintType)
struct FLookReviewMannequin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Review")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Review")
	FTransform Transform;

	/** 반복 재생한다. 비우면 참조 포즈. */
	UPROPERTY(EditAnywhere, Category = "Review")
	TSoftObjectPtr<UAnimationAsset> Animation;
};

/**
 * 룩 비교 캡처를 위한 맵별 준비물. ULookSettings::ReviewSetup 에 걸리면 월드 시작 때
 * ULookSubsystem 이 마네킹을 세우고 페인트를 찍는다. 게임 플레이에는 쓰이지 않는다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API ULookReviewSetup : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Review")
	TSoftObjectPtr<UPaintBrushProfile> Brush;

	/** 스플랫을 찍기까지 기다리는 시간. 월드 시작 직후에는 페인트 표면이 아직 등록되지 않았다. */
	UPROPERTY(EditAnywhere, Category = "Review", meta = (ClampMin = "0", ForceUnits = "s"))
	float SplatDelay = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Review")
	TArray<FLookReviewSplat> Splats;

	UPROPERTY(EditAnywhere, Category = "Review")
	TArray<FLookReviewMannequin> Mannequins;

	/** 비교 캡처가 쓰는 카메라 자리. 게임은 읽지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Review")
	TArray<FTransform> CameraShots;
};
