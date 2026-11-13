// Director's Cut — the single gameplay API used by UI and World (LEAD-OWNED contract).
//
// The PUBLIC section is frozen during Wave 1.
// Agent A (Sim) owns the implementation (Sim/DCRunSubsystem.cpp) and MAY add/change members
// in the PRIVATE section below (and add any helper files under Sim/).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DCTypes.h"
#include "DCRunSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FDCOnRunStateChanged);                                   // anything changed: cash, roster, jobs, rooms...
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPlanCommitted, const FDCCrewPlan&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPlanCancelled, int32 /*JobId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnNightResolved, const FDCNightReport&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDCOnCriminalLevelUp, int32 /*CriminalId*/, int32 /*NewLevel*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRoomChanged, const FDCRoom&);              // built or upgraded
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPerkOffered, const TArray<FDCPerk>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRunEnded, const FDCRunSummary&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnCriminalHired, const FDCCriminal&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnRetakeResolved, const FDCHeistResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDCOnPhaseChanged, EDCGamePhase);

UCLASS()
class DIRECTORSCUT_API UDCRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDCRunSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Run lifecycle ────────────────────────────────────────────────────────
	/** Starts a fresh run. Seed 0 = random seed. Broadcasts OnRunStateChanged + OnPhaseChanged(Planning). */
	void StartNewRun(int32 Seed = 0);
	bool HasActiveRun() const;
	EDCGamePhase GetPhase() const;
	int32 GetSeed() const;
	/** UI calls this after the Dailies reveal finishes (phase Dailies -> Planning / GameOver / Won). */
	void FinishDailies();

	// ── Read state ───────────────────────────────────────────────────────────
	const TArray<FDCCriminal>& GetRoster() const;          // includes injured; excludes arrested
	const TArray<FDCCriminal>& GetFallen() const;          // arrested criminals (for credits)
	const TArray<FDCJob>& GetJobs() const;                 // tonight's board
	const TArray<FDCCriminal>& GetRecruits() const;        // hireable, with HireCost
	TArray<FDCGearItem> GetGearCatalogue() const;          // everything; bAvailable marks purchasable now
	const FDCCriminal* FindCriminal(int32 CriminalId) const;
	const FDCJob* FindJob(int32 JobId) const;
	const FDCGearItem* FindGear(FName ItemId) const;
	FDCStatBlock GetEffectiveStats(int32 CriminalId) const; // base + level + gear + perks
	int32 GetEffectiveLuck(int32 CriminalId) const;

	int32 GetCash() const;
	int32 GetNight() const;                                // 1-based
	int32 GetHeistsCompleted() const;                      // successful heists
	int32 GetUpkeepDue() const;                            // what EndNight will charge
	int32 GetRosterCap() const;
	int32 GetMaxCrewsPerNight() const;
	int32 GetRecruitRefreshIn() const;                     // nights until recruit board refreshes
	int32 GetUnlockedTier() const;

	const TArray<FDCCrewPlan>& GetCommittedPlans() const;
	const FDCNightReport& GetLastNightReport() const;
	FDCRunSummary GetRunSummary() const;

	// ── Planning ─────────────────────────────────────────────────────────────
	/** Pure: computes the full success breakdown for a (possibly incomplete) plan. Never mutates state. */
	FDCSuccessBreakdown PreviewPlan(const FDCCrewPlan& Plan) const;
	/** Commits if PreviewPlan(...).bValid and crew capacity allows. Marks crew OnJob. */
	bool CommitPlan(const FDCCrewPlan& Plan, FString* OutError = nullptr);
	void CancelPlan(int32 JobId);

	// ── Economy ──────────────────────────────────────────────────────────────
	/** Buys ItemId and equips it on CriminalId (replacing whatever was in that slot). */
	bool BuyGear(FName ItemId, int32 CriminalId, FString* OutError = nullptr);
	void Unequip(int32 CriminalId, EDCGearSlot Slot);
	bool Hire(int32 RecruitId, FString* OutError = nullptr);
	void Dismiss(int32 CriminalId);

	// ── Rooms ────────────────────────────────────────────────────────────────
	TArray<FDCRoom> GetRooms() const;
	FDCRoom GetRoom(EDCRoomType Type) const;
	bool IsRoomBuilt(EDCRoomType Type) const;
	bool BuildRoom(EDCRoomType Type, FString* OutError = nullptr);   // broadcasts OnRoomChanged
	bool UpgradeRoom(EDCRoomType Type, FString* OutError = nullptr); // broadcasts OnRoomChanged

	// Room actions
	int32 GetPatchUpCost(int32 CriminalId) const;      // Clinic L2+; -1 if not possible
	bool PatchUp(int32 CriminalId, FString* OutError = nullptr);
	int32 GetBailCost(int32 CriminalId) const;         // Lawyer; CriminalId from GetFallen(); -1 if not possible
	bool BailOut(int32 CriminalId, FString* OutError = nullptr);

	// ── Perks ────────────────────────────────────────────────────────────────
	bool HasPendingPerkOffer() const;
	TArray<FDCPerk> GetPerkOffer() const;
	bool PickPerk(FName PerkId);
	TArray<FDCPerk> GetPerksTaken() const;

	// ── Night ────────────────────────────────────────────────────────────────
	/** "Roll Camera": resolves all committed plans, pays upkeep, ages the world. Phase -> Dailies. */
	TArray<FDCHeistResult> EndNight();
	int32 GetRetakesRemaining() const;
	/** Re-rolls a failed heist from the last night (Screening Room). Returns the new result via OnRetakeResolved. */
	bool UseRetake(int32 HeistResultId, FDCHeistResult* OutNewResult = nullptr);

	// ── Delegates ────────────────────────────────────────────────────────────
	FDCOnRunStateChanged OnRunStateChanged;
	FDCOnPlanCommitted   OnPlanCommitted;
	FDCOnPlanCancelled   OnPlanCancelled;
	FDCOnNightResolved   OnNightResolved;
	FDCOnCriminalLevelUp OnCriminalLevelUp;
	FDCOnRoomChanged     OnRoomChanged;
	FDCOnPerkOffered     OnPerkOffered;
	FDCOnRunEnded        OnRunWon;
	FDCOnRunEnded        OnGameOver;
	FDCOnCriminalHired   OnCriminalHired;
	FDCOnRetakeResolved  OnRetakeResolved;
	FDCOnPhaseChanged    OnPhaseChanged;

private:
	// ═══ Agent A owns everything below this line ═══
	// All logic lives in Sim/ (FDCSimImpl); this section only holds run state.
	friend struct FDCSimImpl;
	friend struct FDCSimTestAccess; // automation tests (Tests/Sim)

	TSharedPtr<struct FDCSimData> Data;   // loaded JSON (Sim/DCSimData.h)
	FString DataDirOverride;              // tests: force a data directory

	bool bActive = false;
	EDCGamePhase Phase = EDCGamePhase::Title;
	int32 Seed = 0;
	FRandomStream Rng;
	int32 Cash = 0;
	int32 Night = 1;
	int32 HeistsCompleted = 0;
	int32 HeistsAttempted = 0;
	int32 TotalTake = 0;
	FString BestHeistName;
	int32 BestHeistTake = 0;
	int32 NextId = 1;
	int32 RecruitRefreshIn = 3;
	int32 PerkOffersGiven = 0;
	int32 LastUnlockedTier = 1;
	int32 RetakesRemaining = 0;
	bool bFirstArrestPrevented = false;
	bool bWonRun = false;
	FString EndReason;

	TArray<FDCCriminal> Roster;
	TArray<FDCCriminal> Fallen;
	TArray<FDCCriminal> Recruits;
	TArray<FDCJob> Jobs;
	TArray<FDCCrewPlan> Plans;
	TArray<FDCRoom> Rooms;               // state + level only; display fields filled by GetRooms()
	TArray<FDCPerk> PerkOffer;
	TArray<FDCPerk> PerksTaken;
	FDCNightReport LastReport;

	TMap<int32, int32> JobRollSeeds;                   // job id -> seed of its heist dice (Dailies perk can peek)
	TMap<int32, int32> ArrestedOnNight;                // fallen criminal id -> night of arrest (Lawyer bail)
	TMap<int32, TArray<FDCCriminal>> RetakeSnapshots;  // failed result id -> crew before the heist
	TMap<int32, FDCJob> RetakeJobs;                    // failed result id -> job as it was
	TMap<int32, FDCSuccessBreakdown> RetakeBreakdowns; // failed result id -> breakdown used

	FDCCriminal* FindCriminalMutable(int32 Id);
};
