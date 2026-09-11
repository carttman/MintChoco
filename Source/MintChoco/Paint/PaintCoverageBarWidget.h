#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "PaintCoverageBarWidget.generated.h"

struct FPaintCoverage;

/** The bar's pure geometry, so the rule that keeps the two fills from overlapping is testable. */
struct MINTCHOCO_API FPaintCoverageBarMath
{
	/**
	 * The two fills as fractions of the bar, 0 to 1. Shares are absolute, so their sum is the
	 * painted part of the world and whatever is left is drawn as the bare middle. Mint keeps its
	 * share and Choco is cut to the space left, so a sum over 1 - which replication can show
	 * briefly while the two values arrive - never draws one fill over the other.
	 */
	static void ComputeFills(float RawMint, float RawChoco, float& OutMint, float& OutChoco);
};

/**
 * The match score as one bar: Mint grows from the left, Choco from the right, and the gap left in
 * the middle is the unpainted remainder, so the three numbers are read from one shape without a
 * legend. The percentages are shares of every paintable surface in the world.
 *
 * The value is the server's cell grid, carried by AGameGameState and replicated every
 * CoverageRefreshInterval, so a client shows exactly what the score will be judged on. That
 * arrives in steps rather than continuously, so the drawn ends chase the replicated value instead
 * of snapping to it.
 *
 * Drawn in NativePaint, so no Blueprint asset is required; a UMG subclass may add a tree of its
 * own on top. Position and size come from the slot it is placed in.
 */
UCLASS()
class MINTCHOCO_API UPaintCoverageBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPaintCoverageBarWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Height of the bar itself. The labels take the space above it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage", meta = (ClampMin = "2", ForceUnits = "px"))
	float BarHeight = 18.0f;

	/** The unpainted remainder, drawn behind both fills. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage")
	FLinearColor TrackColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.55f);

	/** Border drawn around the bar so it stays readable over a bright surface. 0 alpha hides it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage")
	FLinearColor BorderColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage", meta = (ClampMin = "0", ForceUnits = "px"))
	float BorderThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage")
	FSlateFontInfo Font;

	/** Gap between the labels and the bar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage", meta = (ClampMin = "0", ForceUnits = "px"))
	float LabelPadding = 3.0f;

	/**
	 * How fast the drawn ends chase the replicated value, in bar fractions per second. Coverage
	 * arrives in steps, so drawing it raw makes the bar jump; 0 disables the chase.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage", meta = (ClampMin = "0"))
	float InterpSpeed = 1.5f;

	/**
	 * How often the coverage value is read. Matching AGameGameState::CoverageRefreshInterval loses
	 * nothing, since that is how often the number can change at all.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coverage", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float PollInterval = 0.2f;

private:
	/** Server coverage from the game state, or this machine's own grid outside a game map. */
	bool ReadCoverage(float& OutMint, float& OutChoco) const;

	/** Drawn fractions, chasing the polled ones. */
	float MintFraction = 0.0f;
	float ChocoFraction = 0.0f;

	/** Last polled values; the drawn fractions chase these between polls. */
	float TargetMint = 0.0f;
	float TargetChoco = 0.0f;

	float PollTime = 0.0f;
};
