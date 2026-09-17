#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"

#include "MatchResultSettings.generated.h"

class AMatchResultStage;
class UAnimSequence;
class UMaterialInterface;
class UPaintBarWidget;
class UTexture2D;

/**
 * 경기 결과 연출 설정. Config/DefaultGame.ini에 남는다.
 *
 * 시간 값은 전부 FMatchResultTimeline으로 옮겨 담기고, 그 합이 AGameGameMode의 로비 복귀
 * 예약 시간이 된다. 여기서 한 단계를 늘리면 로비 복귀도 같이 늦춰진다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Match Result"))
class MINTCHOCO_API UMatchResultSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UMatchResultSettings& Get() { return *GetDefault<UMatchResultSettings>(); }

	//~ 시간

	/** 경기가 끝나고 화면이 완전히 덮이기까지(초). 아이템 효과가 자연스럽게 끝날 시간이다. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float FreezeSeconds = 3.0f;

	/** 무대가 보이고 빈 게이지로 기다리는 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BarEmptySeconds = 2.0f;

	/** 게이지가 양쪽에 조금 차오른 채 머무는 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BarTeaserSeconds = 1.2f;

	/** 실제 비율로 맞춘 뒤 머무는 시간. 격돌이 보이는 것도 이 동안이다. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BarRealSeconds = 1.5f;

	/** KO 마무리(더 밀기 + 탁해지기)가 올라오는 시간. 바의 KoBlendSeconds와 같게 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BarFinishSeconds = 0.6f;

	/** 승자가 다가오고 패자가 물러나는 데 걸리는 시간. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float CharacterSeconds = 0.6f;

	/** 완성된 그림을 그대로 두는 시간. 이 뒤에 서버가 로비로 보낸다. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float HoldSeconds = 5.0f;

	//~ 게이지

	/** 실제 비율을 보여 주기 전에 양쪽에 채워 보이는 비율. 격돌이 먼저 터지지 않게 ClashCoverage 아래로 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "Bar", meta = (ClampMin = "0", ClampMax = "0.5"))
	float TeaserCoverage = 0.1f;

	/** 결과 화면의 바 크기. 경기 중 HUD의 바보다 크다. */
	UPROPERTY(Config, EditAnywhere, Category = "Bar")
	FVector2D BarSize = FVector2D(1200.0, 44.0);

	/** 화면 아래 끝에서 바까지의 거리(px). 맵과 캐릭터를 가볍게 덮는 자리다. */
	UPROPERTY(Config, EditAnywhere, Category = "Bar", meta = (ClampMin = "0", ForceUnits = "px"))
	float BarBottomOffset = 110.0f;

	/** 결과 바가 값 변화를 따라가는 시간. 경기 중(0.25초)보다 느리게 두어야 차오르는 것이 보인다. */
	UPROPERTY(Config, EditAnywhere, Category = "Bar", meta = (ClampMin = "0", ForceUnits = "s"))
	float BarFillSmoothingSeconds = 0.45f;

	/**
	 * 결과 바로 쓸 위젯. 경기 중 HUD와 같은 WBP_PaintBar를 쓰는 것이 기본이다 - 액체 머티리얼
	 * (BarMaterial)과 물결·격돌 설정이 그 블루프린트에만 있어서, 맨 UPaintBarWidget으로 만들면
	 * 판정선과 링만 그려지고 액체가 통째로 비어 보인다. 크기와 따라가는 속도만 여기서 덮어쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bar")
	TSoftClassPtr<UPaintBarWidget> BarWidgetClass;

	//~ 테두리

	/**
	 * 결과 화면을 두르는 장식 그림. 화면 전체에 늘려 깐다. 비어 있으면 테두리 없이 나머지 연출만 돈다.
	 *
	 * 가운데와 아래가 비어 있는 그림이라야 한다. 점유율 바와 캐릭터를 가리지 않는 것이 전제다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Frame")
	TSoftObjectPtr<UTexture2D> FrameTexture;

	/** 테두리가 밝아지는 시간(초). 이긴 쪽이 승리 모션에 들어가는 순간 시작한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Frame", meta = (ClampMin = "0", ForceUnits = "s"))
	float FrameFadeSeconds = 0.5f;

	//~ 무대

	/** 맵에 무대가 놓여 있지 않을 때 대신 스폰할 클래스. 비어 있으면 AMatchResultStage를 그대로 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	TSoftClassPtr<AMatchResultStage> FallbackStageClass;

	/** 서서 기다리는 동안 도는 애니메이션. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;

	/** 이긴 쪽이 다가온 뒤 도는 애니메이션. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	TSoftObjectPtr<UAnimSequence> WinnerAnimation;

	/** 이긴 쪽이 카메라 쪽으로 다가오는 거리(cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Stage", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WinnerStepCm = 90.0f;

	/** 진 쪽이 카메라에서 물러나는 거리(cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Stage", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LoserStepCm = 70.0f;

	//~ 회색조

	/**
	 * 진 쪽만 회색으로 만드는 포스트프로세스 머티리얼. 결과 카메라에만 얹히므로 경기 중 화면에는
	 * 영향이 없다. SceneTexture:PostProcessInput0을 CustomStencil이 LoserStencilValue인 곳에서만
	 * 회색으로 바꾸는 머티리얼이다. 비어 있으면 회색조 없이 나머지 연출만 돈다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Grayscale")
	TSoftObjectPtr<UMaterialInterface> LoserDesaturateMaterial;

	/** 진 쪽 메시에 쓰는 커스텀 스텐실 값. 프로젝트 설정의 Custom Depth-Stencil Pass가 켜져 있어야 한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Grayscale", meta = (ClampMin = "1", ClampMax = "255"))
	int32 LoserStencilValue = 1;
};
