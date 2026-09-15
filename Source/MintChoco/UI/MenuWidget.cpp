#include "UI/MenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"

#include "Audio/AudioGameplayTags.h"
#include "Audio/GameAudioSubsystem.h"

void UMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bAutoButtonSounds || !WidgetTree)
	{
		return;
	}

	// 이름 슬롯 안의 자식 위젯 트리는 훑지 않는다: 그쪽도 UMenuWidget이면 스스로 묶는다.
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UButton* const Button = Cast<UButton>(Widget);
		if (!Button || BoundButtons.Contains(Button) || !ShouldAutoSound(Button))
		{
			return;
		}
		Button->OnClicked.AddDynamic(this, &UMenuWidget::HandleButtonClicked);
		Button->OnHovered.AddDynamic(this, &UMenuWidget::HandleButtonHovered);
		BoundButtons.Add(Button);
	});
}

void UMenuWidget::HandleButtonClicked()
{
	UGameAudioSubsystem::Play2D(this, AudioTags::Audio_UI_Click);
}

void UMenuWidget::HandleButtonHovered()
{
	UGameAudioSubsystem::Play2D(this, AudioTags::Audio_UI_Hover);
}
