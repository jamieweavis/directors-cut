// Director's Cut — the single gameplay API used by UI, World and Audio (LEAD-OWNED contract, v2 realtime).
//
// The PUBLIC section is frozen during Wave R1.
// Agent A (Sim) owns the implementation (Sim/*) and MAY add or change members in the PRIVATE section below.
//
// Realtime model:
//  - The subsystem ticks itself (FTickableGameObject). While a run is Playing and not paused, RunTime advances by
//    DeltaTime * DevTimeScale * GameSpeed (1x / 2x / 4x). Every timer (missions, builds, injuries, board expiry, recruit refresh) is in run seconds.
//  - Clock: hour = StartHour + RunTime * GameMinutesPerSecond / 60 (default 1 game minute per real second, so a day is
//    24 real minutes). Payday (upkeep) is charged at every dawn (DawnHour).
//  - Saving: autosave every AutosaveInterval s, on every mission start/complete/build, and on Deinitialize.
//    Closing the game freezes time: Continue restores RunTime exactly (no offline progress). GameOver/Won keep the save, marked finished.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Core/DCTypes.h"
#include "DCRunSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FDCOnRunStateChanged);                                    // anything changed (cash, roster, board, rooms...)
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPhaseChanged, EDCGamePhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPauseChanged, bool /*bPaused*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDCOnGameSpeedChanged, int32 /*SpeedIndex*/, bool /*bSpeedPaused*/);
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
	// ── Save slots + leaderboard (v3) ────────────────────────────────────────
	// Every run lives in one of NumSaveSlots slots and autosaves there. A finished run (GameOver/Won) is NOT deleted:
	// it stays in its slot marked finished. StartNewRun() / HasSave() / ContinueRun() keep working: they use the
	// most recently saved unfinished slot (StartNewRun: the first empty slot, else the oldest).
	static constexpr int32 NumSaveSlots = 3;
	TArray<FDCSaveSlotInfo> GetSaveSlots() const;          // always NumSaveSlots entries
	int32 GetCurrentSlot() const;                          // slot the loaded run saves to, -1 if none
	int32 GetMostRecentSlot(bool bUnfinishedOnly = true) const; // -1 if none
	/** Fresh run in Slot (overwrites it). Phase -> Playing. */
	void StartNewRunInSlot(int32 Slot, int32 Seed = 0);
	/** Loads Slot. Unfinished -> Playing; finished -> its end phase (GameOver/Won) with GetRunSummary() valid. */
	bool LoadSlot(int32 Slot);
	/** Copies the loaded run (active or finished) into Slot, which becomes the current slot. */
	bool SaveToSlot(int32 Slot);
	bool DeleteSlot(int32 Slot);
	/** Finished runs, best first: wins by fastest PlayTime, then losses by DaysSurvived, then TotalTake. Persistent. */
	TArray<FDCRunRecord> GetLeaderboard() const;
	void ClearLeaderboard();

	int32 GetSeed() const;

	/** The Esc menu pauses the clock. Broadcasts OnPauseChanged. */
	void SetPaused(bool bInPaused);
	bool IsPaused() const;
	// ── Game speed (player controls) ─────────────────────────────────────────
	// Scales how fast run time follows real time: 1x / 2x / 4x, plus a player pause that is separate from the
	// Esc-menu pause above (closing the menu never resumes a player pause). Not saved: every new run and every
	// load starts at 1x, running. AdvanceTime() (tests, bots) is unaffected.
	static constexpr int32 NumGameSpeeds = 3;
	/** Multiplier for a speed index: 0 = 1x, 1 = 2x, 2 = 4x. */
	static float GameSpeedMultiplier(int32 SpeedIndex);
	/** Sets the speed and resumes from a player pause. Broadcasts OnGameSpeedChanged. */
	void SetGameSpeed(int32 SpeedIndex);
	int32 GetGameSpeed() const;
	float GetGameSpeedMultiplier() const;
	/** Player pause: run time stops, the speed is kept for when it resumes. */
	void SetSpeedPaused(bool bInPaused);
	void ToggleSpeedPaused();
	bool IsSpeedPaused() const;
	/** Run seconds per real second right now: dev time scale x game speed, 0 while either pause holds. */
	float GetEffectiveTimeScale() const;
	FDCOnGameSpeedChanged OnGameSpeedChanged;

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
	int32 GetBoardSize() const;                            // max postings on the board (excludes the Big Score)
	int32 GetUpkeepDue() const;                            // charged at next dawn
	double GetRecruitRefreshIn() const;                    // run seconds
	int32 GetRecruitRefreshCost() const;                   // pay to reroll now
	double GetNextMissionIn() const;                       // run seconds until the next posting (board not full)
	/** Run seconds until each empty board slot fills, soonest first; empty when the board is full.
	 *  Postings arrive one per post interval, so slot i is due at the next post time + i intervals. */
	TArray<double> GetPendingPostingTimes() const;
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
	/** Starts a build (Locked) or upgrade (Built, <3) timer and pays the cost. One build at a time.
	 *  A first build needs a grid cell (DCSite::Grid::CanPlace); without one the default spot is picked. */
	bool BuildRoom(EDCRoomType Type, EDCPlanBlock* OutBlock = nullptr, const FIntPoint* Cell = nullptr);
	/** Grid cells taken by rooms (placed = built, being built, or the Office). */
	TArray<FIntPoint> GetOccupiedCells() const;
	/** Where a room stands on the build grid (false = not placed yet). Cheap; safe every frame. */
	bool GetRoomCell(EDCRoomType Type, FIntPoint& OutCell) const;
	bool IsAnyRoomBuilding() const;
	/** Rooms that may be under construction at once (1, +buildSlots perks). */
	int32 GetBuildSlots() const;
	/** True when every build slot is taken. */
	bool IsBuildCapacityFull() const;

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
	/** The current offer holds every perk still obtainable: the unpicked ones are discarded for the run. */
	bool IsFinalPerkOffer() const;
	/** Successful missions in a row (a failure or bust resets it). */
	int32 GetSuccessStreak() const { return SuccessStreak; }

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
	int32 GameSpeed = 0;                  // index into GameSpeedMultiplier
	bool bSpeedPaused = false;            // player pause (Space / HUD), separate from the Esc-menu pause
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
	TArray<FName> PerksDiscarded;         // left over from the final draft: never offered again this run

	// timers (run seconds) and run bookkeeping
	double NextPostAt = 0.0;
	double RecruitRefreshAt = 0.0;
	double NextAutosaveAt = 0.0;
	int32 LastDawnIndex = 0;
	int32 MissionsSucceeded = 0;
	int32 MissionsAttempted = 0;
	int32 TotalTake = 0;
	int32 BestMissionTake = 0;
	FString BestMissionName;
	int32 RoomsBuilt = 0;
	int32 PendingPerkOffers = 0;          // queued offers beyond the current one
	bool bArrestPreventUsed = false;
	int32 SuccessStreak = 0;
	FString EndReason;
	TMap<int32, double> ArrestedAt;      // fallen criminal id -> run time of arrest (bail window)

	bool bSaveEnabled = true;             // tests/bots may disable disk writes
	bool bSaveDirty = false;              // save at the end of the current AdvanceTime
	bool bStateDirty = false;             // broadcast OnRunStateChanged at the end of the current AdvanceTime
	int32 AdvanceDepth = 0;

	void EnsureData();
	void ResetRunState();
	void MarkChanged();
	void FlushChanged();

	// ── save slots + leaderboard (v3) ──
	int32 CurrentSlot = -1;               // slot the loaded run saves to
	FGuid RunId;                          // unique per run; dedupes leaderboard records across slot copies
	mutable TArray<FDCSaveSlotInfo> SlotCache;   // headers of all slots (refreshed on save/delete, not per frame)
	mutable bool bSlotCacheValid = false;
	mutable bool bLegacyChecked = false;
	mutable TArray<FDCRunRecord> LeaderboardCache;
	mutable TArray<FGuid> LeaderboardIds;  // parallel to LeaderboardCache
	mutable bool bLeaderboardLoaded = false;

	FString GetSlotPrefix() const;        // "DCRun" (or the test override)
	FString GetSlotName(int32 Slot) const;
	FString GetLeaderboardSlotName() const;
	bool IsFinished() const { return Phase == EDCGamePhase::GameOver || Phase == EDCGamePhase::Won; }
	void EnsureSlotCache() const;
	void MigrateLegacySave() const;
	FDCSaveSlotInfo MakeSlotInfo(int32 Slot) const;  // from the in-memory run
	bool WriteSlot(int32 Slot);           // writes the in-memory run (active or finished) to Slot
	bool ApplySave(const class UDCSaveGame& SG);
	void EnsureLeaderboard() const;
	void RecordFinishedRun();             // EndRun: writes the slot as finished + one leaderboard record
};
