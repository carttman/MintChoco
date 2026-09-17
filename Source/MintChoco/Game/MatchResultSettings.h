#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Math/Interval.h"
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

	/**
	 * 완성된 그림을 그대로 두는 시간. 이 뒤에 서버가 로비로 보낸다.
	 *
	 * 보통 이 자리는 승리 모션의 길이 x WinnerAnimationLoops 가 대신한다. 이 값이 쓰이는 것은
	 * WinnerAnimation 이 비었거나 길이를 읽지 못했을 때뿐이다.
	 */
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

	/**
	 * 테두리가 나타나는 동안 가장 부풀었을 때의 크기 배율. 1 이면 크기를 건드리지 않고 밝아지기만 한다.
	 *
	 * 화면을 덮는 그림이라 부풀면 가장자리가 화면 밖으로 밀렸다 돌아온다. 테두리 그림에 여백이 없으면
	 * 그만큼 잘려 보이므로, 그림을 바꿀 때 같이 맞춘다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Frame", meta = (ClampMin = "1", ClampMax = "2"))
	float FramePeakScale = 1.15f;

	/** 가장 부푸는 순간(밝아지는 시간에 대한 비율). 앞쪽일수록 빠르게 부풀고 천천히 내려앉는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Frame", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float FramePeakAt = 0.35f;

	//~ 스티커

	/**
	 * 승리 순간에 화면 위에서 쏟아지는 과자 스티커. 한 장짜리 스프라이트 시트다.
	 *
	 * 비어 있으면 스티커 없이 나머지 연출만 돈다. 칸은 왼쪽 위에서 가로로 세고, 빈 칸이 있으면
	 * 그 칸도 뽑히므로 시트는 칸을 꽉 채워 둔다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker")
	TSoftObjectPtr<UTexture2D> StickerTexture;

	UPROPERTY(Config, EditAnywhere, Category = "Sticker", meta = (ClampMin = "1"))
	int32 StickerColumns = 5;

	UPROPERTY(Config, EditAnywhere, Category = "Sticker", meta = (ClampMin = "1"))
	int32 StickerRows = 3;

	/** 배율 1 인 스티커 한 장의 한 변(px). */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker", meta = (ClampMin = "1", ForceUnits = "px"))
	float StickerSize = 112.0f;

	/** 한 번의 연출에서 뿌리는 총 장수. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker", meta = (ClampMin = "0", ClampMax = "512"))
	int32 StickerCount = 72;

	/** 이 시간에 걸쳐 고르게 나눠 뿌린다. 0 이면 첫 프레임에 다 나가 한 번에 터진다. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker", meta = (ClampMin = "0", ForceUnits = "s"))
	float StickerSpawnSeconds = 3.0f;

	/** 한 장이 화면에 머무는 시간(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker")
	FFloatInterval StickerLife = FFloatInterval(2.6f, 4.2f);

	/** 기준 크기에 곱하는 배율. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker")
	FFloatInterval StickerScale = FFloatInterval(0.62f, 1.35f);

	/**
	 * 아래로 내려가는 처음 속도. 화면 높이를 1 로 본 초당 거리라 해상도가 달라도 같은 그림이 나온다.
	 * 아래의 흔들림·가속도 같은 자를 쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion")
	FFloatInterval StickerFallSpeed = FFloatInterval(0.16f, 0.38f);

	/** 떨어지면서 붙는 가속(화면 높이/초²). */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion", meta = (ClampMin = "0"))
	float StickerGravity = 0.20f;

	/** 옆으로 새는 처음 속도의 폭(화면 너비/초). 좌우 양쪽으로 같은 폭만큼 흩어진다. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion", meta = (ClampMin = "0"))
	float StickerSideDrift = 0.05f;

	/** 좌우로 흔들리는 폭(화면 너비/초). 이것이 0 이면 그냥 곧게 떨어진다. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion")
	FFloatInterval StickerSwayAmplitude = FFloatInterval(0.03f, 0.10f);

	/** 흔들리는 주기(라디안/초). */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion")
	FFloatInterval StickerSwayRate = FFloatInterval(1.5f, 3.6f);

	/** 도는 속도(도/초). 절반은 반대로 돈다. */
	UPROPERTY(Config, EditAnywhere, Category = "Sticker|Motion")
	FFloatInterval StickerSpin = FFloatInterval(25.0f, 130.0f);

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

	/**
	 * 승리 모션을 몇 번 돌리고 로비로 보낼지. 마지막 Hold 단계의 길이가 이 값에서 나온다.
	 *
	 * WinnerAnimation 의 길이를 읽어 곱하므로, 애니메이션을 바꾸면 기다리는 시간도 같이 바뀐다.
	 * 애니메이션이 비었거나 길이가 0 이면 HoldSeconds 를 그대로 쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Stage", meta = (ClampMin = "1", ClampMax = "10"))
	int32 WinnerAnimationLoops = 2;

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

	//~ 건너뛰기

	/**
	 * 연출을 건너뛰고 로비로 빠지는 키. 누른 사람만 떠나고 다른 머신의 연출은 그대로 돈다.
	 *
	 * 에디터에서는 Esc 가 PIE 를 멈추고 물결표가 콘솔을 여는 등 뷰포트가 먼저 가져가는 키가 있다.
	 * 패키징한 빌드에서만 온전히 동작하는 키라면 여기서 다른 키로 바꾼다. 비워 두면 건너뛰기가 없다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Skip")
	TArray<FKey> SkipKeys = { EKeys::Escape, EKeys::One };

	/**
	 * 건너뛴 사람이 가는 맵. 서버의 로비 복귀와 달리 이 머신만 떠나는 클라이언트 트래블이다.
	 *
	 * 리슨 서버의 호스트가 누르면 서버가 함께 내려가므로 붙어 있던 전원이 튕긴다. 호스트가 경기를
	 * 그만두는 것과 같은 일이고, 그것이 싫으면 호스트는 끝까지 보거나 이 목록에서 키를 뺀다.
	 * 비워 두면 건너뛰기가 없다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Skip")
	FString SkipTravelURL = TEXT("/Game/Maps/Lobby");
};
