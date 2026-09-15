#pragma once

#include "CoreMinimal.h"
#include "UI/MenuWidget.h"

#include "TitleWidget.generated.h"

class UWidget;

/**
 * 타이틀 로고가 떨어지는 연출의 시계. UObject가 아니라 테스트가 위젯 없이 돌릴 수 있다.
 *
 * WaitingForScreen → Delaying → Dropping → Landed 순으로 돈다. 맵 로드 직후에는 가림막
 * (UScreenFadeSubsystem)이 덮고 있으므로 화면이 열리기 시작할 때까지 기다린다. 가림막 아래에서
 * 끝나 버리면 아무도 못 본다.
 */
struct MINTCHOCO_API FTitleLogoDrop
{
	enum class EPhase : uint8
	{
		/** 가림막이 아직 덮고 있다. 로고는 보이지 않는다. */
		WaitingForScreen,
		/** 화면이 열리기 시작했고 Delay만큼 더 기다린다. 로고는 보이지 않는다. */
		Delaying,
		/** 떨어지고 튀는 중. */
		Dropping,
		/** 제자리에 섰다. */
		Landed
	};

	/** 떨어지기 시작해서 마지막 튐이 멈출 때까지(초). */
	float Duration = 1.2f;

	/** 화면이 열리기 시작한 뒤 떨어지기 전까지(초). */
	float Delay = 0.2f;

	/** 튈 때 남는 속도의 비율. 낮을수록 빨리 잦아든다. 0이면 튀지 않고 선다. */
	float Restitution = 0.4f;

	/** 바닥에 닿은 뒤 튀는 횟수. */
	int32 Bounces = 2;

	EPhase Phase = EPhase::WaitingForScreen;

	/** 현재 단계에 들어온 뒤 지난 시간(초). */
	float Elapsed = 0.0f;

	/** 처음부터 다시. 화면이 열리기를 기다리는 데서 시작한다. */
	void Restart();

	/**
	 * 한 틱 진행한다. bScreenVisible은 가림막이 걷혔거나 걷히는 중인지다. 한 틱에 흐르는 시간은
	 * 1/30초로 자른다: 첫 프레임의 긴 멈춤(셰이더 컴파일)이 연출을 통째로 삼키지 않게.
	 * 이번 틱에 단계가 바뀌었으면 true.
	 */
	bool Tick(float DeltaTime, bool bScreenVisible);

	/** 로고가 보여야 하는지. 떨어지기 전에는 숨긴다. */
	bool IsLogoVisible() const { return Phase == EPhase::Dropping || Phase == EPhase::Landed; }

	/** 지금 높이. 0 = 착지점, 1 = 시작 높이(화면 밖). */
	float GetHeight() const;

	/**
	 * 정규화한 시간 Alpha(0..1)에서의 높이(0..1). 공을 떨어뜨린 것처럼 중력으로 가속해 떨어지고,
	 * 튈 때마다 속도가 Restitution배가 되어 튀는 높이는 Restitution²배씩 낮아진다. Bounces번 튄 뒤
	 * Alpha 1에서 정확히 멈추고, 착지점 아래로는 내려가지 않는다.
	 */
	static float EvaluateHeight(float Alpha, float Restitution, int32 Bounces);
};

/**
 * 타이틀 화면 위젯(WBP_Title의 부모).
 *
 * 맵에 들어오면 로고(Title_Logo)가 화면 위에서 떨어져 몇 번 잦아들며 튀고 제자리에 선다.
 * 레이아웃은 건드리지 않고 렌더 트랜스폼의 이동만 쓰므로, 디자이너가 캔버스에 놓은 자리가
 * 그대로 착지점이다. 연출 값은 WBP_Title의 클래스 기본값(Title|Logo Drop)에서 고친다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UTitleWidget : public UMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 떨어지는 로고. 캔버스에 놓인 자리가 착지점이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Title_Logo;

	/** 떨어지기 시작해서 마지막 튐이 멈출 때까지(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Title|Logo Drop", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float DropDuration = 1.2f;

	/** 가림막이 걷히기 시작한 뒤 떨어지기 전까지(초). 페이드 인은 0.5초다. */
	UPROPERTY(EditDefaultsOnly, Category = "Title|Logo Drop", meta = (ClampMin = "0", ForceUnits = "s"))
	float DropDelay = 0.2f;

	/** 튈 때 남는 속도의 비율. 낮을수록 부드럽게 잦아든다. 0이면 튀지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Title|Logo Drop", meta = (ClampMin = "0", ClampMax = "0.9"))
	float DropRestitution = 0.4f;

	/** 바닥에 닿은 뒤 튀는 횟수. */
	UPROPERTY(EditDefaultsOnly, Category = "Title|Logo Drop", meta = (ClampMin = "0", ClampMax = "5"))
	int32 DropBounces = 2;

private:
	/** 로고의 아래 끝이 이 위젯의 위쪽 밖으로 나가려면 올려야 하는 거리(로고 부모의 단위). */
	float MeasureDropDistance(const FGeometry& MyGeometry) const;

	FTitleLogoDrop Drop;

	/** 떨어지기 시작할 때 잰 거리. 높이 1이 이만큼 위다. */
	float DropDistance = 0.0f;

	/** 디자이너가 정한 로고의 불투명도와 이동. 숨겼다가 되돌리고, 이동은 여기에 더한다. */
	float RestOpacity = 1.0f;
	FVector2D RestTranslation = FVector2D::ZeroVector;
	bool bRestCaptured = false;

	/** 착지 자세를 이미 적용했는지. 그 뒤로는 틱에서 아무것도 하지 않는다. */
	bool bDropFinished = false;
};
