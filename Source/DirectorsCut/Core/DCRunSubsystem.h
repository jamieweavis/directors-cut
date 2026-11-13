// Director's Cut — the single gameplay API used by UI, World and Audio (LEAD-OWNED contract, v2 realtime).
//
// The PUBLIC section is frozen during Wave R1.
// Agent A (Sim) owns the implementation (Sim/*) and MAY add or change members in the PRIVATE section below.
//
// Realtime model (PLAN.md §R):
//  - The subsystem ticks itself (FTickableGameObject). While a run is Playing and not paused, RunTime advances by
//    DeltaTime * DevTimeScale. Every timer (missions, builds, injuries, board expiry, recruit refresh) is in run seconds.
//  - Clock: hour = StartHour + RunTime * GameMinutesPerSecond / 60 (default 1 game minute per real second, so a day is
//    24 real minutes). Payday (upkeep) is charged at every dawn (DawnHour).
//  - Saving: autosave every AutosaveInterval s, on every mission start/complete/build, and on Deinitialize.
//    Closing the game freezes time: Continue restores RunTime exactly (no offline progress). GameOver/Won delete the save.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Core/DCTypes.h"
#include "DCRunSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FDCOnRunStateChanged);                                    // anything changed (cash, roster, board, rooms...)
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPhaseChanged, EDCGamePhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPauseChanged, bool /*bPaused*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnMissionStarted, const FDCMission&);        // crew leaves (bInProgress = true)
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnMissionCompleted, const FDCMissionReport&); // crew returns; state already applied
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnMissionAdded, const FDCMission&);          // new mission posted on the board
DECLARE_MULTICAST_DELEGATE_TwoParams(FDCOnCriminalLevelUp, int32 /*CriminalId*/, int32 /*NewLevel*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRoomChanged, const FDCRoom&);              // build/upgrade STARTED (State=Building) or FINISHED (State=Built)
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPerkOffered, const TArray<FDCPerk>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRunEnded, const FDCRunSummary&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnCriminalHired, const FDCCriminal&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnCriminalHealed, int32 /*CriminalId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDCOnPayday, int32 /*Day*/, int32 /*AmountPaid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnDayStarted, int32 /*Day*/);                // at dawn, after payday

UCLASS()
class DIRECTORSCUT_API UDCRunSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UDCRunSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickableWhenPaused() const override { return false; }

	// ── Run lifecycle ────────────────────────────────────────────────────────
	/** Fresh run (overwrites any save). Seed 0 = random. Phase -> Playing. */
	void StartNewRun(int32 Seed = 0);
	/** True if a saved, unfinished run exists on disk. */
	bool HasSave() const;
	/** Loads the saved run. Phase -> Playing. Returns false if there's no save or it's corrupt. */
	bool ContinueRun();
	/** Writes the save now (no-op without an active run). */
	void SaveRun();
	/** Saves, then returns to Title (phase Title, no active run in memory). */
	void ExitToTitle();
	/** Deletes the save, ends the run without a summary, phase Title. */
	void AbandonRun();
	bool HasActiveRun() const;
	EDCGamePhase GetPhase() const;
	int32 GetSeed() const;

	/** The Esc menu pauses the clock. Broadcasts OnPauseChanged. */
	void SetPaused(bool bInPaused);
	bool IsPaused() const;
	/** Dev/testing only (-DCTimeScale=N); 1 = real time. */
	void SetTimeScale(float Scale);
	float GetTimeScale() const;
	/** Tests/bots: advance the simulation by Seconds, processing every timer in order. */
	void AdvanceTime(double Seconds);

	// ── Clock ────────────────────────────────────────────────────────────────
	double GetRunTime() const;                 // run seconds since the run started (frozen while paused/closed)
	int32 GetDay() const;                      // 1-based; increments at midnight
	float GetHourOfDay() const;                // 0..24 (fractional)
	bool IsNight() const;                      // between DuskHour and DawnHour
	float GetSunAlpha() const;                 // 0 at night .. 1 at noon (smooth); convenience for lighting/audio
	double GetSecondsUntilPayday() const;      // run seconds until the next dawn
	float GetGameMinutesPerSecond() const;     // clock scale (data), default 1

	// ── Read state ───────────────────────────────────────────────────────────
	const TArray<FDCCriminal>& GetRoster() const;          // includes injured / on mission; excludes arrested
	const TArray<FDCCriminal>& GetFallen() const;          // arrested
	const TArray<FDCMission>& GetMissions() const;         // board + in progress (bInProgress)
	const TArray<FDCCriminal>& GetRecruits() const;        // hireable, with HireCost
	TArray<FDCGearItem> GetGearCatalogue() const;          // shop; bAvailable marks purchasable now
	const TArray<FName>& GetStash() const;                 // owned, unequipped gear (loot + unequipped)
	const FDCCriminal* FindCriminal(int32 CriminalId) const;
	const FDCMission* FindMission(int32 MissionId) const;
	const FDCGearItem* FindGear(FName ItemId) const;
	const FDCAbility* FindAbility(FName AbilityId) const;
	const FDCTrait* FindTrait(FName TraitId) const;
	const FDCPerk* FindPerk(FName PerkId) const;
	/** Level + gear Power + perks. */
	float GetEffectiveLevel(int32 CriminalId) const;
	/** Innate + gear-granted traits. */
	TArray<FName> GetAllTraits(int32 CriminalId) const;
	/** Abilities' threats + gear counters. */
	TArray<EDCThreat> GetCounters(int32 CriminalId) const;
	int32 GetGearSlotCount() const;                        // 2, or 3 with the Workshop

	int32 GetCash() const;
	int32 GetMissionsSucceeded() const;
	int32 GetUnlockedTier() const;
	int32 GetRosterCap() const;
	int32 GetMaxActiveMissions() const;
	int32 GetActiveMissionCount() const;
	int32 GetUpkeepDue() const;                            // charged at next dawn
	double GetRecruitRefreshIn() const;                    // run seconds
	int32 GetRecruitRefreshCost() const;                   // pay to reroll now
	double GetNextMissionIn() const;                       // run seconds until the next posting (board not full)
	/** Seconds left / progress 0..1 for an in-progress mission (0 / 0 if not running). */
	double GetMissionTimeRemaining(int32 MissionId) const;
	float GetMissionProgress(int32 MissionId) const;
	FDCRunSummary GetRunSummary() const;

	// ── Missions ─────────────────────────────────────────────────────────────
	/** Pure: full breakdown for a (possibly incomplete) plan. Never mutates state. */
	FDCSuccessBreakdown PreviewPlan(const FDCCrewPlan& Plan) const;
	/** Picks the best available crew for a mission (greedy: counters first, then level). For the UI's auto-fill button + bots. */
	FDCCrewPlan SuggestPlan(int32 MissionId, float Cut = 0.3f) const;
	/** Pays Cost, locks the chance, marks crew OnMission, sets EndsAt. Broadcasts OnMissionStarted. */
	bool StartMission(const FDCCrewPlan& Plan, EDCPlanBlock* OutBlock = nullptr);

	/** Completed missions, newest first. */
	const TArray<FDCMissionReport>& GetReports() const;
	int32 GetUnseenReportCount() const;
	void MarkReportSeen(int32 ReportId);
	void MarkAllReportsSeen();

	// ── Economy ──────────────────────────────────────────────────────────────
	/** Buys ItemId from the shop and equips it on CriminalId (the old item goes to the stash). */
	bool BuyGear(FName ItemId, int32 CriminalId, FString* OutError = nullptr);
	/** Equips an item from the stash. */
	bool EquipFromStash(FName ItemId, int32 CriminalId, FString* OutError = nullptr);
	void Unequip(int32 CriminalId, EDCGearSlot Slot);      // to the stash
	bool SellFromStash(FName ItemId);                      // 50% refund
	bool Hire(int32 RecruitId, FString* OutError = nullptr);
	void Dismiss(int32 CriminalId);
	bool RefreshRecruits();                                // pays GetRecruitRefreshCost

	// ── Rooms ────────────────────────────────────────────────────────────────
	TArray<FDCRoom> GetRooms() const;
	FDCRoom GetRoom(EDCRoomType Type) const;
	bool IsRoomBuilt(EDCRoomType Type) const;              // Level >= 1 (still true while upgrading)
	/** Starts a build (Locked) or upgrade (Built, <3) timer and pays the cost. One build at a time. */
	bool BuildRoom(EDCRoomType Type, EDCPlanBlock* OutBlock = nullptr);
	bool IsAnyRoomBuilding() const;

	// Room actions
	int32 GetPatchUpCost(int32 CriminalId) const;          // Clinic L2+; -1 if not possible
	bool PatchUp(int32 CriminalId);
	int32 GetBailCost(int32 CriminalId) const;             // Lawyer; id from GetFallen(); -1 if not possible
	bool BailOut(int32 CriminalId);

	// ── Perks ────────────────────────────────────────────────────────────────
	bool HasPendingPerkOffer() const;
	TArray<FDCPerk> GetPerkOffer() const;
	bool PickPerk(FName PerkId);
	TArray<FDCPerk> GetPerksTaken() const;

	// ── Delegates ────────────────────────────────────────────────────────────
	FDCOnRunStateChanged  OnRunStateChanged;
	FDCOnPhaseChanged     OnPhaseChanged;
	FDCOnPauseChanged     OnPauseChanged;
	FDCOnMissionStarted   OnMissionStarted;
	FDCOnMissionCompleted OnMissionCompleted;
	FDCOnMissionAdded     OnMissionAdded;
	FDCOnCriminalLevelUp  OnCriminalLevelUp;
	FDCOnRoomChanged      OnRoomChanged;
	FDCOnPerkOffered      OnPerkOffered;
	FDCOnRunEnded         OnRunWon;
	FDCOnRunEnded         OnGameOver;
	FDCOnCriminalHired    OnCriminalHired;
	FDCOnCriminalHealed   OnCriminalHealed;
	FDCOnPayday           OnPayday;
	FDCOnDayStarted       OnDayStarted;

private:
	// ═══ Agent A owns everything below this line ═══
	friend struct FDCSimImpl;
	friend struct FDCSimTestAccess; // automation tests (Tests/Sim)

	TSharedPtr<struct FDCSimData> Data;   // loaded JSON
	FString DataDirOverride;              // tests: force a data directory
	FString SaveSlotOverride;             // tests: use a separate save slot

	bool bActive = false;
	bool bPaused = false;
	float TimeScale = 1.f;
	EDCGamePhase Phase = EDCGamePhase::Title;
	int32 Seed = 0;
	FRandomStream Rng;
	double RunTime = 0.0;
	int32 Cash = 0;
	int32 NextId = 1;

	TArray<FDCCriminal> Roster;
	TArray<FDCCriminal> Fallen;
	TArray<FDCCriminal> Recruits;
	TArray<FDCMission> Missions;
	TArray<FDCMissionReport> Reports;
	TArray<FName> Stash;
	TArray<FDCRoom> Rooms;
	TArray<FDCPerk> PerkOffer;
	TArray<FDCPerk> PerksTaken;
};
