#include "Weapons/PaintCrosshairHostWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"

#include "Game/Unit.h"
#include "MintChoco.h"
#include "Weapons/PaintBracketCrosshairWidget.h"
#include "Weapons/PaintCrosshairWidget.h"
#include "Weapons/PaintWeaponComponent.h"
#include "Weapons/PaintWeaponProfile.h"

UPaintCrosshairHostWidget::UPaintCrosshairHostWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultCrosshairClass = UPaintBracketCrosshairWidget::StaticClass();
}

TSubclassOf<UPaintCrosshairWidget> UPaintCrosshairHostWidget::ResolveCrosshairClass(const UPaintWeaponProfile* Profile, TSubclassOf<UPaintCrosshairWidget> Default)
{
	if (Profile && Profile->CrosshairClass)
	{
		return Profile->CrosshairClass;
	}
	return Default;
}

UPaintWeaponComponent* UPaintCrosshairHostWidget::PickActiveWeapon(const APawn* Pawn)
{
	const AUnit* const Unit = Cast<AUnit>(Pawn);
	if (!Unit)
	{
		return nullptr;
	}
	UPaintWeaponComponent* const Secondary = Unit->GetSecondaryWeapon();
	if (Secondary && (Secondary->IsTriggerHeld() || Secondary->IsAiming()))
	{
		return Secondary;
	}
	return Unit->GetPaintWeapon();
}

TSharedRef<SWidget> UPaintCrosshairHostWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	return Super::RebuildWidget();
}

void UPaintCrosshairHostWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

UPaintCrosshairWidget* UPaintCrosshairHostWidget::FindOrCreate(TSubclassOf<UPaintCrosshairWidget> Class)
{
	if (!Class)
	{
		return nullptr;
	}
	if (const TObjectPtr<UPaintCrosshairWidget>* const Found = Instances.Find(Class.Get()))
	{
		return Found->Get();
	}

	UCanvasPanel* const Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas)
	{
		UE_LOG(LogMintChoco, Warning, TEXT("%s: root is not a CanvasPanel, cannot host %s."), *GetName(), *Class->GetName());
		return nullptr;
	}
	UPaintCrosshairWidget* const Widget = CreateWidget<UPaintCrosshairWidget>(this, Class);
	if (!Widget)
	{
		// 추상 클래스를 골랐거나 만들 수 없는 클래스. 매 틱 다시 시도하지 않게 null 을 기억한다.
		UE_LOG(LogMintChoco, Warning, TEXT("%s: could not create crosshair %s."), *GetName(), *Class->GetName());
		Instances.Add(Class.Get(), nullptr);
		return nullptr;
	}
	if (UCanvasPanelSlot* const CanvasSlot = Canvas->AddChildToCanvas(Widget))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}
	Instances.Add(Class.Get(), Widget);
	return Widget;
}

void UPaintCrosshairHostWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* const PlayerController = GetOwningPlayer();
	const APawn* const Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UPaintWeaponComponent* const Active = PickActiveWeapon(Pawn);

	// 폰이 없으면(관전, 리스폰 대기) 아무것도 보이지 않는다.
	UPaintCrosshairWidget* Current = nullptr;
	if (Pawn)
	{
		const UPaintWeaponProfile* const Profile = Active ? Active->GetProfile() : nullptr;
		Current = FindOrCreate(ResolveCrosshairClass(Profile, DefaultCrosshairClass));
	}

	for (const TPair<TObjectPtr<UClass>, TObjectPtr<UPaintCrosshairWidget>>& Pair : Instances)
	{
		UPaintCrosshairWidget* const Widget = Pair.Value.Get();
		if (!Widget)
		{
			continue;
		}
		if (Widget == Current)
		{
			// 접혀 있던 위젯은 틱이 돌지 않으므로 먼저 펴야 페이드 인이 시작된다.
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			Widget->SetWeapon(Active);
			Widget->SetShown(true);
		}
		else
		{
			Widget->SetShown(false);
			if (Widget->IsFadedOut())
			{
				Widget->SetWeapon(nullptr);
				Widget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}
