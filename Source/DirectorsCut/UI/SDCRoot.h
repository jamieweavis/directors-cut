// Director's Cut — root Slate widget hosting the HUD, panels, modals and toasts (Agent C).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/DCTypes.h"

class UDCUISubsystem;
class UDCRunSubsystem;
class SBox;
class SBorder;
class SVerticalBox;
class SScrollBox;

class SDCRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDCRoot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UDCUISubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SDCRoot();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// ── Driven by UDCUISubsystem ────────────────────────────────────────────
	void Open(EDCPanel Panel);
	void OpenPlanner(int32 JobId);
	/** bForce: also closes modal-ish panels that normally refuse (game over while phase GameOver, etc). */
	void Close(bool bForce = false);
	EDCPanel GetCurrent() const { return Current; }
	bool IsWorldBlocked() const;
	void Back();
	void RollCamera(bool bFromButton);
	void Toast(const FString& Msg, bool bWarn);
	void ShowStartScreen();

	static bool IsModal(EDCPanel P);

private:
	TWeakObjectPtr<UDCUISubsystem> Owner;
	UDCRunSubsystem* Run() const;
	bool HasRun() const;

	// Layers
	TSharedPtr<SBorder> TopBar;
	TSharedPtr<SBorder> PanelFrame;
	TSharedPtr<SBox> PanelSlot;
	TSharedPtr<SBorder> ModalFrame;
	TSharedPtr<SBox> ModalSlot;
	TSharedPtr<SVerticalBox> ToastBox;
	TSharedPtr<SBorder> TutorialStrip;

	EDCPanel Current = EDCPanel::None;
	EDCPanel ReturnPanel = EDCPanel::None; // where Settings returns to
	bool bPanelShown = false;
	bool bModalShown = false;
	double PanelT0 = -10.0;  // time panel layer last opened/closed
	double ModalT0 = -10.0;
	double DisplayCash = 0.0;
	bool bCashInit = false;
	bool bDirty = false;
	bool bSliderDragging = false;
	void SetCurrent(EDCPanel P);
	double LastRollClick = -10.0;

	// Scroll position preservation across rebuilds
	TArray<TSharedPtr<SScrollBox>> Scrolls;
	TArray<float> SavedScroll;
	TSharedRef<SWidget> Scroll(TSharedRef<SWidget> Content, bool bOnPaper = false);

	// Toasts
	struct FToast { TSharedPtr<SWidget> Widget; double Time = 0.0; };
	TArray<FToast> Toasts;

	// Tutorial
	int32 TutorialStep = -1; // -1 = hidden
	void AdvanceTutorial(int32 FromStep);
	void DismissTutorial();

	// Delegates
	FDelegateHandle HState, HCommit, HNight, HPerk, HOver, HWon, HRoom, HHire, HLevel;
	void BindRun();
	void UnbindRun();
	void OnRunState();
	void OnNightResolved(const struct FDCNightReport& R);
	void OnPerkOffered(const TArray<FDCPerk>& Perks);
	void OnGameOver(const FDCRunSummary& S);
	void OnRunWon(const FDCRunSummary& S);

	// Build
	void Rebuild();
	TSharedRef<SWidget> BuildTopBar();
	TSharedRef<SWidget> BuildTutorial();
	TSharedRef<SWidget> BuildPanel(EDCPanel P);
	TSharedRef<SWidget> PanelShell(const FString& Title, const FString& Subtitle, TSharedRef<SWidget> Body);

	// Non-modal panels (SDCRootPanels.cpp)
	TSharedRef<SWidget> BuildJobBoard();
	TSharedRef<SWidget> BuildPlanner();
	TSharedRef<SWidget> BuildWorkbench();
	TSharedRef<SWidget> BuildRoster();
	TSharedRef<SWidget> BuildDesk();
	TSharedRef<SWidget> BuildBlueprint();
	TSharedRef<SWidget> BuildScreening();
	TSharedRef<SWidget> BuildClinic();
	TSharedRef<SWidget> BuildLawyer();
	TSharedRef<SWidget> BuildServer();
	TSharedRef<SWidget> RoomNotBuilt(EDCRoomType Room);
	TSharedRef<SWidget> CriminalCard(const FDCCriminal& C, bool bCompact);
	TSharedRef<SWidget> JobCard(const FDCJob& J);

	// Crew planner state
	int32 PlanJobId = INDEX_NONE;
	TArray<int32> PlanCrew;
	float PlanCut = 0.30f;
	TSharedPtr<SVerticalBox> BreakdownBox;
	struct FPlanCache;
	TSharedPtr<FPlanCache> PlanCache;
	void RecomputePlan();
	void ToggleCrew(int32 CriminalId);

	// Workbench state
	int32 BenchCriminal = INDEX_NONE;

	// Modals (SDCRootModals.cpp)
	TSharedRef<SWidget> BuildTitle();
	TSharedRef<SWidget> BuildSettings();
	TSharedRef<SWidget> BuildDailies();
	TSharedRef<SWidget> BuildDailiesCard();
	TSharedRef<SWidget> BuildDailiesSummary();
	TSharedRef<SWidget> BuildPerkDraft();
	TSharedRef<SWidget> BuildGameOver();
	TSharedRef<SWidget> BuildCredits();
	void StartNewRun(int32 Seed = 0);
	void FinishDailiesFlow();

	// Dailies state
	struct FDailiesState;
	TSharedPtr<FDailiesState> Dailies;
	TSharedPtr<SBox> DailiesSlot;
	void DailiesNext();
	void DailiesRetake();
	void DailiesTick();

	// Pending modals after Dailies
	bool bPerkPending = false;
	FDCRunSummary EndSummary;
	bool bHasEndSummary = false;

	// Settings state
	int32 ResIndex = -1;
};
