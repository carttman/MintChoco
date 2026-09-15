#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "PaintCrosshairHostWidget.generated.h"

class APawn;
class UPaintCrosshairWidget;
class UPaintWeaponComponent;
class UPaintWeaponProfile;

/**
 * 어느 크로스헤어를 보여 줄지 정하는 자리. 매 틱 소유 플레이어의 폰에서 활성 무기를 고르고
 * (보조무기의 방아쇠가 당겨져 있으면 보조, 아니면 주무기), 그 무기 프로필의 CrosshairClass
 * (비어 있으면 DefaultCrosshairClass)에 맞는 크로스헤어 인스턴스를 클래스별로 하나씩 만들어
 * 풀스크린으로 겹쳐 두고, 맞는 것만 보이게 페이드한다. 그래서 우클릭을 누르는 순간 주무기의
 * 브래킷이 사라지고 보조무기의 스코프가 뜨며, 놓으면 되돌아온다.
 *
 * 풀스크린 캔버스 슬롯에 놓는 것을 전제로 한다(자식 크로스헤어가 제 크기의 절반을 화면
 * 중앙으로 쓰기 때문). WBP_GameHUD 에 없으면 UGameHudWidget 이 코드로 붙인다.
 */
UCLASS()
class MINTCHOCO_API UPaintCrosshairHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPaintCrosshairHostWidget(const FObjectInitializer& ObjectInitializer);

	/** 프로필이 고른 클래스, 없으면 기본. 프로필이 없어도 기본. */
	static TSubclassOf<UPaintCrosshairWidget> ResolveCrosshairClass(const UPaintWeaponProfile* Profile, TSubclassOf<UPaintCrosshairWidget> Default);

	/** 크로스헤어가 따라갈 무기: 보조무기가 방아쇠를 당기거나 조준 중이면 보조, 아니면 주무기. 유닛이 아니면 null. */
	static UPaintWeaponComponent* PickActiveWeapon(const APawn* Pawn);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 프로필이 크로스헤어를 고르지 않았을 때. 기본은 브래킷. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	TSubclassOf<UPaintCrosshairWidget> DefaultCrosshairClass;

private:
	/** 클래스별로 한 번만 만든다. 루트 캔버스에 풀스크린으로 들어간다. */
	UPaintCrosshairWidget* FindOrCreate(TSubclassOf<UPaintCrosshairWidget> Class);

	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, TObjectPtr<UPaintCrosshairWidget>> Instances;
};
