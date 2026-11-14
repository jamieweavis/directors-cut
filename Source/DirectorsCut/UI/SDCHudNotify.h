// Director's Cut — bottom-right mission-result cards: animated roll-out-of-100, outcome and payout (Agent C).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/DCTypes.h"

class SVerticalBox;

/**
 * Stack of mission-result cards. Push() one per completed mission; newest sits at the bottom.
 * Each card sweeps a marker across the roll bar, settles on the roll, reveals the outcome and payout or
 * consequence, then dismisses itself. Timers hold while the card is hovered or the stack is Suspended
 * (a panel or modal is open), and a card goes away once its report has been seen in the Reports panel.
 */
class SDCHudNotify : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDCHudNotify) : _Suspended(false) {}
		/** True while something covers the corner (panels, modals): cards fade out and their timers hold. */
		SLATE_ATTRIBUTE(bool, Suspended)
		/** Card clicked: open the Reports panel (report id passed for context). */
		SLATE_ARGUMENT(TFunction<void(int32)>, OnOpenReport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void Push(const FDCMissionReport& Report);

	static constexpr int32 MaxVisible = 4;
	static constexpr float Lifetime = 7.5f;   // seconds each card stays (excluding held time)

private:
	struct FCard
	{
		int32 ReportId = INDEX_NONE;
		TSharedPtr<SWidget> Widget;
		TSharedPtr<SWidget> HoverTarget;      // the clickable card; hovering holds the timer
		TSharedPtr<float> Age;                // shared with the card's animation lambdas; < 0 = waiting its turn
	};
	TArray<FCard> Cards;
	TSharedPtr<SVerticalBox> Stack;
	TAttribute<bool> Suspended;
	TFunction<void(int32)> OnOpenReport;
	float ShowAlpha = 1.f;                    // fades the whole stack while suspended
	double LastPushAt = -10.0;
	float LastPushDelay = 0.f;

	TSharedRef<SWidget> BuildCard(const FDCMissionReport& Rep, TSharedRef<float> Age, TSharedPtr<SWidget>& OutHoverTarget);
	void Remove(int32 Index);
	void AddDemoCards();                      // dev-only (-DCNotifyDemo): sample cards for screenshots
	bool bDemoPending = false;
	double DemoAt = 0.0;
};
