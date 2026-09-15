#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MenuWidget.generated.h"

class UButton;

/**
 * 메뉴 화면 위젯의 공통 부모(타이틀, 룸, 로비, 팝업, 결과창).
 *
 * 만들어질 때 자기 위젯 트리의 모든 UButton에 클릭음(Audio.UI.Click)과 호버음(Audio.UI.Hover)을
 * 묶는다. 버튼이 늘어도 블루프린트에서 소리를 따로 연결할 일이 없고, 소리 파일은 사운드 뱅크가
 * 정한다. 자기 소리를 따로 내는 버튼(참가, 생성)은 ShouldAutoSound에서 뺀다.
 *
 * 블루프린트가 NativeConstruct를 덮어써도 부모 호출만 하면 된다. 이벤트 그래프의 Construct는
 * NativeConstruct 뒤에 오므로 순서 문제가 없다.
 */
UCLASS(Abstract)
class MINTCHOCO_API UMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	/** 이 버튼에 자동 소리를 붙일지. 기본은 전부. 자기 소리를 내는 버튼은 false를 돌려준다. */
	virtual bool ShouldAutoSound(const UButton* Button) const { return true; }

	/** 끄면 이 위젯의 버튼은 자동 소리를 받지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Menu|Audio")
	bool bAutoButtonSounds = true;

private:
	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	/** 이미 묶은 버튼. 같은 위젯이 다시 Construct돼도(뷰포트 재추가) 두 번 묶지 않는다. */
	UPROPERTY(Transient)
	TSet<TObjectPtr<UButton>> BoundButtons;
};
