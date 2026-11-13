// Director's Cut — root Slate widget: HUD, dock, panels, modals and toasts (Agent C).
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
	void OpenPlanner(int32 MissionId);
	/** bForce also closes screens that normally only leave through their own buttons. */
	void Close(bool bForce = false);
	EDCPanel GetCurrent() const { return Current; }
	bool IsWorldBlocked() const;
	void Back();
	void Toast(const FString& Msg, FName Icon, bool bWarn);
	/** Hotkey from the input preprocessor. Returns true if it did something. */
	bool HandleHotkey(const FKey& Key);

	static bool IsModal(EDCPanel P);

private:
	TWeakObjectPtr<UDCUISubsystem> Owner;
	UDCRunSubsystem* Run() const;
	bool HasRun() const;
	bool IsPlaying() const;

	// Layers
	TSharedPtr<SBox> PanelSlot;
	TSharedPtr<SBox> ModalSlot;
	TSharedPtr<SVerticalBox> ToastBox;
	TSharedPtr<SVerticalBox> ChipBox;

	EDCPanel Current = EDCPanel::None;
	EDCPanel Shown = EDCPanel::None;   // the panel whose content is in PanelSlot (kept during fade-out)
	EDCPanel ShownModal = EDCPanel::None;
	bool bPanelShown = false;
	bool bModalShown = false;
	double PanelT0 = -10.0;
	double ModalT0 = -10.0;
	void SetCurrent(EDCPanel P);

	// Live data / rebuild
	double DisplayCash = 0.0;
	bool bCashInit = false;
	double CashFlashT0 = -10.0;
	bool bCashFlashUp = true;
	uint32 LastSig = 0;
	uint32 LastChipSig = 0;
	double NextSigCheck = 0.0;
	uint32 ComputeSignature() const;
	uint32 ComputeChipSignature() const;
	void Rebuild();
	void RebuildChips();

	// Scroll position preservation across rebuilds
	TArray<TSharedPtr<SScrollBox>> Scrolls;
	TArray<float> SavedScroll;
	TSharedRef<SWidget> Scroll(TSharedRef<SWidget> Content);

	// Toasts
	struct FToast { TSharedPtr<SWidget> Widget; double Time = 0.0; };
	TArray<FToast> Toasts;

	// Badges / seen state
	TSet<int32> SeenMissionIds;          // board postings the player has looked at
	TMap<int32, double> FirstSeenAt;     // mission id -> run time when first seen (expiry bars)
	int32 NewPostingCount() const;
	bool CanBuildSomething() const;

	// Delegates
	FDelegateHandle HCompleted, HAdded, HLevel, HRoom, HPerk, HOver, HWon, HPayday, HPhase, HHired, HStarted, HHealed;
	void BindRun();
	void UnbindRun();
	void OnMissionCompleted(const FDCMissionReport& R);
	void OnPerkOffered(const TArray<FDCPerk>& Perks);
	void OnRunEnded(const FDCRunSummary& S, bool bWon);
	void OnPhaseChanged(EDCGamePhase P);

	// ── Build ───────────────────────────────────────────────────────────────
	TSharedRef<SWidget> BuildHudClock();
	TSharedRef<SWidget> BuildHudStats();
	TSharedRef<SWidget> BuildDock();
	TSharedRef<SWidget> DockButton(FName Icon, const FString& Name, const FString& Key, EDCPanel Panel, TAttribute<int32> BadgeCount, TAttribute<bool> Enabled = true);
	TSharedRef<SWidget> MissionChip(const FDCMission& M);
	TSharedRef<SWidget> BuildChip(const FDCRoom& R);
	TSharedRef<SWidget> BuildPanel(EDCPanel P);
	float PanelWidth(EDCPanel P) const;
	/** Panel header: icon tile, title, optional right-side widget, close button. */
	TSharedRef<SWidget> Shell(FName Icon, const FString& Title, TSharedRef<SWidget> Body, TSharedPtr<SWidget> HeaderRight = nullptr, const FString& Sub = FString());

	// Panels (SDCPanelMissions.cpp)
	TSharedRef<SWidget> BuildMissions();
	TSharedRef<SWidget> MissionCard(const FDCMission& M);
	TSharedRef<SWidget> ActiveMissionRow(const FDCMission& M);
	TSharedRef<SWidget> BuildPlanner();
	TSharedRef<SWidget> PlannerRunning(const FDCMission& M);
	TSharedRef<SWidget> BuildReports();
	TSharedRef<SWidget> ReportCard(const FDCMissionReport& R);
	TSharedRef<SWidget> ThreatRow(const TArray<EDCThreat>& Threats, float Size);

	// Planner state
	int32 PlanMissionId = INDEX_NONE;
	TArray<int32> PlanCrew;
	float PlanCut = 0.3f;
	struct FPlanCache;
	TSharedPtr<FPlanCache> PlanCache;
	void RecomputePlan();
	void ToggleCrew(int32 CriminalId);
	void AutoFill();
	void StartPlan();
	TMap<int32, double> ReportAnimT0;   // report id -> reveal start (so rebuilds don't restart animations)
	TSet<int32> ReportStung;            // outcome sting already played

	// Panels (SDCPanelCrew.cpp)
	TSharedRef<SWidget> BuildCrew();
	TSharedRef<SWidget> CrewCard(const FDCCriminal& C, bool bRecruit);
	TSharedRef<SWidget> CrewDetail(const FDCCriminal& C);
	TSharedRef<SWidget> AbilityChips(const FDCCriminal& C, float Size, const TArray<EDCThreat>* Highlight = nullptr);
	TSharedRef<SWidget> TraitChips(const TArray<FName>& Traits, float Size);
	TSharedRef<SWidget> GearSlots(const FDCCriminal& C, float Size);
	TSharedRef<SWidget> StatusChip(const FDCCriminal& C);
	TSharedRef<SWidget> CrewTip(const FDCCriminal& C) const;
	TSharedRef<SWidget> BuildWorkbench();
	TSharedRef<SWidget> GearCard(const FDCGearItem& G, bool bStash);
	TSharedRef<SWidget> BuildRecruits();
	int32 CrewSel = INDEX_NONE;
	int32 GearSel = INDEX_NONE;
	int32 DismissArmed = INDEX_NONE;

	// Panels (SDCPanelMeta.cpp)
	TSharedRef<SWidget> BuildBuild();
	TSharedRef<SWidget> RoomTile(const FDCRoom& R);
	TSharedRef<SWidget> BuildRoomPanel(EDCPanel P);
	TSharedRef<SWidget> RoomLocked(EDCRoomType Room);
	TSharedRef<SWidget> BuildPerkDraft();
	TSharedRef<SWidget> BuildTitle();
	TSharedRef<SWidget> BuildPause();
	TSharedRef<SWidget> BuildEnd(bool bWon);
	TSharedRef<SWidget> StatTile(FName Icon, const FString& Value, const FString& Caption, const FLinearColor& Col);
	void NewRun();
	bool bAbandonArmed = false;
	FDCRunSummary EndSummary;
	bool bHasEndSummary = false;
	bool bPerkQueued = false;
};
