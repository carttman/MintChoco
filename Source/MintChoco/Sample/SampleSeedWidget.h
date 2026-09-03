#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

#include "SampleSeedWidget.generated.h"

class UCheckBox;
class UEditableTextBox;

/**
 * Debug control for the splat seed: the text box pins every splat to one seed, the checkbox
 * returns to a fresh random seed per splat. The widget builds its own tree in C++, so no
 * Blueprint asset is required; a UMG subclass can restyle it by providing widgets named
 * SeedInput and AutoSeedCheck.
 */
UCLASS()
class MINTCHOCO_API USampleSeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Mirrors the seed the next splat will use. Skipped while the user is typing in the box. */
	void SetDisplayedSeed(int32 Seed);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SeedInput;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> AutoSeedCheck;

private:
	UFUNCTION()
	void OnSeedCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void OnAutoSeedChanged(bool bIsChecked);

	void BuildDefaultTree();
	void PushToController();
};
