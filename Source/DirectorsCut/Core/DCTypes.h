// Director's Cut — shared data contracts, v2 "realtime crew & missions" (LEAD-OWNED, frozen during Wave R1).
// Agents: do not edit. Request changes in your final report.
//
// Design: PLAN.md §R. A dark, modern crime-management game played in real time. Crew (followers) run timed missions
// from a warehouse hideout via a mission table. Abilities counter threats, traits add bonuses,
// and crew level is weighed against mission level.
//
// TIME: every timer is in RUN SECONDS (real seconds the run has been unpaused; see UDCRunSubsystem::GetRunTime).
// The game clock maps run time to time of day: one real second is one game minute at the default scale (a 24-min day).
#pragma once

#include "CoreMinimal.h"
#include "DCTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

/** Crew archetype ("class"). Decides which abilities and traits a criminal can roll. */
UENUM(BlueprintType)
enum class EDCCriminalType : uint8
{
	Wheelman,
	Muscle,
	Hacker,
	Ghost,
	Face,
	Safecracker,
	MAX UMETA(Hidden)
};

/** Mission threats. Each ability or gear counter neutralises one threat instance. */
UENUM(BlueprintType)
enum class EDCThreat : uint8
{
	Guards,     // armed security
	Cameras,    // CCTV
	Alarms,     // alarm systems / silent alarms
	Locks,      // doors, padlocks, cages
	Vault,      // safes, vaults, strongboxes
	Patrols,    // police patrols, roaming eyes
	Witnesses,  // civilians, staff, nosy neighbours
	Pursuit,    // the chase afterwards
	MAX UMETA(Hidden)
};

/** Follower quality: more abilities and traits as it rises. Street = 1 ability + 1 trait, Pro = 1 + 2, Elite = 2 + 3. */
UENUM(BlueprintType)
enum class EDCQuality : uint8
{
	Street,
	Pro,
	Elite,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCGearSlot : uint8
{
	Tool,    // usually grants a threat counter
	Outfit,  // usually grants a trait
	Gadget,  // usually adds Power (effective level)
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCCriminalStatus : uint8
{
	Available,
	OnMission,  // ActiveMissionId != INDEX_NONE
	Injured,    // until InjuredUntil (run seconds)
	Arrested    // gone (moved to Fallen), unless the Lawyer bails them out
};

UENUM(BlueprintType)
enum class EDCMissionOutcome : uint8
{
	None,
	Success,  // payout (bonus if chance > 100% rolled the overflow)
	Failed,   // no payout, injury chance
	Busted    // failed badly: injuries + arrest chance
};

UENUM(BlueprintType)
enum class EDCGamePhase : uint8
{
	Title,     // no run loaded
	Playing,   // run active; the clock ticks unless paused
	GameOver,  // went broke (save deleted)
	Won        // pulled off The Big Score (save deleted)
};

UENUM(BlueprintType)
enum class EDCRoomType : uint8
{
	Office,        // start: mission board + desk (recruits, finances)
	Lounge,        // bunks: roster cap, idle loyalty
	Workshop,      // gear catalogue, gear slots
	Garage,        // shown as the Kit Room: concurrent missions, shorter durations
	OpsRoom,       // surveillance wall: tier 3+ missions, The Big Score
	Clinic,        // faster injury recovery, patch-ups
	ServerRoom,    // bigger board, faster board refresh, Hacker bonus
	LawyerOffice,  // bail, lower arrest chance
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCRoomState : uint8
{
	Locked,    // dark, sheeted, tape
	Building,  // timer running (first build OR upgrade; see FDCRoom::Level), scaffolding + work lights
	Built      // lit, Level 1..3
};

UENUM(BlueprintType)
enum class EDCStationType : uint8
{
	Desk,          // Office: recruits + finances
	MissionBoard,  // Office: wall of screens/map, the mission table
	Lounge,        // roster
	Workbench,     // gear
	GarageDoor,    // missions in progress
	BuildTable,    // Office: build/upgrade rooms + perks
	OpsRoom,
	Clinic,
	ServerRack,
	Lawyer
};

/** Panels the UI can open. World (stations) calls UDCUISubsystem::OpenPanel. */
UENUM(BlueprintType)
enum class EDCPanel : uint8
{
	None,
	Title,           // continue / new run / settings / quit
	Missions,        // board + active missions -> pick one -> MissionPlanner
	MissionPlanner,  // fill slots, cut, odds, start
	Crew,            // roster
	Workbench,       // gear shop + loadouts
	Recruits,        // desk: hire + finances
	Build,           // rooms + perks
	PerkDraft,
	Reports,         // completed mission results (unseen first)
	GameOver,
	Victory,
	Pause,           // Esc menu (pauses the clock) + settings
	OpsRoom,
	Clinic,
	Lawyer,
	ServerRoom
};

UENUM(BlueprintType)
enum class EDCVolumeChannel : uint8
{
	Master,
	SFX,
	Ambience
};

// ─────────────────────────────────────────────────────────────────────────────
// Static definitions (from Content/Data JSON)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FDCAbility
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;   // "Loop the Feed"
	UPROPERTY(BlueprintReadOnly) FName Icon;            // Lucide icon name, e.g. "cctv"
	UPROPERTY(BlueprintReadOnly) EDCThreat Counters = EDCThreat::Guards;
};

USTRUCT(BlueprintType)
struct FDCTrait
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;   // "Night Owl"
	UPROPERTY(BlueprintReadOnly) FString Description;   // short: "+15% at night"
	UPROPERTY(BlueprintReadOnly) FName Icon;            // Lucide icon name
	UPROPERTY(BlueprintReadOnly) bool bNegative = false;
};

USTRUCT(BlueprintType)
struct FDCPerk
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;   // short, one line
	UPROPERTY(BlueprintReadOnly) FName Icon;
};

USTRUCT(BlueprintType)
struct FDCGearItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;    // short
	UPROPERTY(BlueprintReadOnly) FName Icon;
	UPROPERTY(BlueprintReadOnly) EDCGearSlot Slot = EDCGearSlot::Tool;
	UPROPERTY(BlueprintReadOnly) int32 Price = 0;         // after discounts
	UPROPERTY(BlueprintReadOnly) float Power = 0.f;       // + effective level
	UPROPERTY(BlueprintReadOnly) EDCThreat Counters = EDCThreat::MAX; // MAX = none
	UPROPERTY(BlueprintReadOnly) FName TraitId;           // granted trait (NAME_None = none)
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;
	UPROPERTY(BlueprintReadOnly) bool bAvailable = true;  // purchasable now
	UPROPERTY(BlueprintReadOnly) FString UnavailableReason; // "Workshop L2", "$ short"
};

// ─────────────────────────────────────────────────────────────────────────────
// Run state
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FDCCriminal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString FirstName;
	UPROPERTY(BlueprintReadOnly) FString Nickname;
	UPROPERTY(BlueprintReadOnly) FString LastName;
	UPROPERTY(BlueprintReadOnly) EDCCriminalType Type = EDCCriminalType::Muscle;
	UPROPERTY(BlueprintReadOnly) EDCQuality Quality = EDCQuality::Street;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;           // cosmetic variation (pawn look, portrait)
	UPROPERTY(BlueprintReadOnly) bool bFemale = false;     // picks the body (DCPaths::CrewBodiesFemale) and first name
	UPROPERTY(BlueprintReadOnly) int32 Body = INDEX_NONE;  // index into CrewBodies / CrewBodiesFemale (by bFemale); none = by Id (old saves)

	UPROPERTY(BlueprintReadOnly) int32 Level = 1;          // 1..MaxLevel
	UPROPERTY(BlueprintReadOnly) int32 XP = 0;             // progress toward next level
	UPROPERTY(BlueprintReadOnly) int32 XPToNext = 100;
	UPROPERTY(BlueprintReadOnly) int32 Loyalty = 60;       // 0..100

	UPROPERTY(BlueprintReadOnly) TArray<FName> Abilities;  // FDCAbility ids
	UPROPERTY(BlueprintReadOnly) TArray<FName> Traits;     // FDCTrait ids (innate; gear traits not included)
	UPROPERTY(BlueprintReadOnly) TMap<EDCGearSlot, FName> Equipped;

	UPROPERTY(BlueprintReadOnly) EDCCriminalStatus Status = EDCCriminalStatus::Available;
	UPROPERTY(BlueprintReadOnly) int32 ActiveMissionId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) double InjuredUntil = 0.0; // run seconds

	UPROPERTY(BlueprintReadOnly) int32 HireCost = 0;       // recruits only
	UPROPERTY(BlueprintReadOnly) int32 MissionsRun = 0;

	FString GetDisplayName() const { return FString::Printf(TEXT("%s \"%s\" %s"), *FirstName, *Nickname, *LastName); }
	FString GetShortName() const { return Nickname.IsEmpty() ? FirstName : Nickname; }
	bool IsAvailable() const { return Status == EDCCriminalStatus::Available; }
};

/** A mission on the board (or running). */
USTRUCT(BlueprintType)
struct FDCMission
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName TemplateId;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Flavour;          // ONE short line
	UPROPERTY(BlueprintReadOnly) FName Icon;               // Lucide icon name
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;           // 1..4
	UPROPERTY(BlueprintReadOnly) int32 Level = 1;          // mission level vs crew effective level
	UPROPERTY(BlueprintReadOnly) int32 CrewSize = 1;       // every slot must be filled
	UPROPERTY(BlueprintReadOnly) float Duration = 60.f;    // run seconds at start, before modifiers (preview has the final)
	UPROPERTY(BlueprintReadOnly) TArray<EDCThreat> Threats; // duplicates allowed
	UPROPERTY(BlueprintReadOnly) int32 Cost = 0;           // paid upfront at start
	UPROPERTY(BlueprintReadOnly) int32 Payout = 0;         // total take on success
	UPROPERTY(BlueprintReadOnly) int32 XPReward = 50;      // per member on success
	UPROPERTY(BlueprintReadOnly) float ArrestRisk = 0.f;   // per member, on Busted
	UPROPERTY(BlueprintReadOnly) bool bNight = false;      // night job: bonus if started at night (moon icon)
	UPROPERTY(BlueprintReadOnly) bool bRare = false;       // highlighted, better loot
	UPROPERTY(BlueprintReadOnly) bool bFinal = false;      // The Big Score: success wins the run
	UPROPERTY(BlueprintReadOnly) FName LootGearId;         // gear dropped on success (NAME_None = none)
	UPROPERTY(BlueprintReadOnly) double ExpiresAt = -1.0;  // run seconds; <0 = never. Ignored once started

	// running state (valid when bInProgress)
	UPROPERTY(BlueprintReadOnly) bool bInProgress = false;
	UPROPERTY(BlueprintReadOnly) double StartedAt = 0.0;   // run seconds
	UPROPERTY(BlueprintReadOnly) double EndsAt = 0.0;      // run seconds
	UPROPERTY(BlueprintReadOnly) TArray<int32> CrewIds;
	UPROPERTY(BlueprintReadOnly) float Cut = 0.3f;
	UPROPERTY(BlueprintReadOnly) float LockedChance = 0.f; // success chance frozen at start
};

USTRUCT(BlueprintType)
struct FDCCrewPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) int32 MissionId = INDEX_NONE;
	UPROPERTY(BlueprintReadWrite) TArray<int32> CriminalIds;   // <= CrewSize; must equal CrewSize to start
	UPROPERTY(BlueprintReadWrite) float Cut = 0.3f;            // Director's cut 0.10..0.90, steps of 0.05
};

/** One threat on a mission and who (if anyone) counters it in a preview. */
USTRUCT(BlueprintType)
struct FDCThreatSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EDCThreat Threat = EDCThreat::Guards;
	UPROPERTY(BlueprintReadOnly) int32 CounteredBy = INDEX_NONE; // criminal id
	UPROPERTY(BlueprintReadOnly) FName Source;                   // ability id or gear id
	bool IsCountered() const { return CounteredBy != INDEX_NONE; }
};

/** A trait or perk that changed the odds/reward/duration in a preview. */
USTRUCT(BlueprintType)
struct FDCModifierHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;                 // trait, perk or room id ("room.Garage")
	UPROPERTY(BlueprintReadOnly) FName Icon;
	UPROPERTY(BlueprintReadOnly) FString Label;            // short: "Night Owl"
	UPROPERTY(BlueprintReadOnly) int32 CriminalId = INDEX_NONE; // source criminal, or INDEX_NONE
	UPROPERTY(BlueprintReadOnly) float ChanceDelta = 0.f;  // +0.15
	UPROPERTY(BlueprintReadOnly) float DurationMult = 1.f; // 0.8
	UPROPERTY(BlueprintReadOnly) float PayoutMult = 1.f;   // 1.2
	UPROPERTY(BlueprintReadOnly) bool bNegative = false;
};

/** Validity reason, so the UI can show an icon instead of a sentence. */
UENUM(BlueprintType)
enum class EDCPlanBlock : uint8
{
	None,
	SlotsEmpty,
	CrewUnavailable,
	Refusal,        // someone refuses at this cut
	NoCash,         // can't pay the mission cost
	NoCapacity,     // max concurrent missions reached
	Expired,
	Locked          // tier/room requirement
};

USTRUCT(BlueprintType)
struct FDCSuccessBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bValid = false;
	UPROPERTY(BlueprintReadOnly) EDCPlanBlock Block = EDCPlanBlock::SlotsEmpty;
	UPROPERTY(BlueprintReadOnly) TArray<FDCThreatSlot> Threats;
	UPROPERTY(BlueprintReadOnly) TMap<int32, float> MemberFactor;   // per criminal: level factor 0.1..1.2
	UPROPERTY(BlueprintReadOnly) TMap<int32, float> Willingness;    // per criminal 0..1
	UPROPERTY(BlueprintReadOnly) TArray<int32> RefusingIds;
	UPROPERTY(BlueprintReadOnly) TArray<FDCModifierHit> Modifiers;  // traits, perks, rooms, night

	UPROPERTY(BlueprintReadOnly) float CrewChance = 0.f;      // from filled slots x level factor
	UPROPERTY(BlueprintReadOnly) float ThreatChance = 0.f;    // from countered threats
	UPROPERTY(BlueprintReadOnly) float ModifierChance = 0.f;  // summed ChanceDelta
	UPROPERTY(BlueprintReadOnly) float WillFactor = 1.f;      // greedy cut penalty multiplier
	UPROPERTY(BlueprintReadOnly) float SuccessChance = 0.f;   // final, 0..2 (above 1 = bonus odds)
	UPROPERTY(BlueprintReadOnly) float BonusChance = 0.f;     // chance of the bonus payout (overflow above 1)
	UPROPERTY(BlueprintReadOnly) float ArrestChance = 0.f;    // per member if Busted
	UPROPERTY(BlueprintReadOnly) float Duration = 0.f;        // run seconds, after modifiers
	UPROPERTY(BlueprintReadOnly) int32 Cost = 0;
	UPROPERTY(BlueprintReadOnly) int32 Payout = 0;            // after modifiers (no bonus)
	UPROPERTY(BlueprintReadOnly) int32 DirectorTake = 0;      // Payout * Cut
};

USTRUCT(BlueprintType)
struct FDCCrewMemberResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 CriminalId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) bool bInjured = false;
	UPROPERTY(BlueprintReadOnly) float InjurySeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bArrested = false;
	UPROPERTY(BlueprintReadOnly) bool bArrestPrevented = false;
	UPROPERTY(BlueprintReadOnly) int32 XPGained = 0;
	UPROPERTY(BlueprintReadOnly) bool bLevelledUp = false;
	UPROPERTY(BlueprintReadOnly) int32 NewLevel = 1;
	UPROPERTY(BlueprintReadOnly) int32 LoyaltyDelta = 0;
	UPROPERTY(BlueprintReadOnly) bool bQuit = false;          // loyalty fell below the quit line: they walked out (gear -> stash)
	UPROPERTY(BlueprintReadOnly) int32 LoyaltyAfter = 0;
	UPROPERTY(BlueprintReadOnly) EDCCriminalType Type = EDCCriminalType::Muscle; // for the avatar once they're gone
	UPROPERTY(BlueprintReadOnly) EDCQuality Quality = EDCQuality::Street;
};

/** Result of a finished mission. Applied to state at completion; the UI reveals it from GetReports(). */
USTRUCT(BlueprintType)
struct FDCMissionReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 ReportId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FDCMission Mission;          // as it was when started (CrewIds, Cut, LockedChance)
	UPROPERTY(BlueprintReadOnly) float Roll = 0.f;            // 0..1
	UPROPERTY(BlueprintReadOnly) EDCMissionOutcome Outcome = EDCMissionOutcome::None;
	UPROPERTY(BlueprintReadOnly) bool bBonus = false;
	UPROPERTY(BlueprintReadOnly) int32 PayoutTotal = 0;
	UPROPERTY(BlueprintReadOnly) int32 DirectorTake = 0;
	UPROPERTY(BlueprintReadOnly) FName LootGearId;            // dropped into the stash (NAME_None = none)
	UPROPERTY(BlueprintReadOnly) TArray<FDCCrewMemberResult> Members;
	UPROPERTY(BlueprintReadOnly) double CompletedAt = 0.0;
	UPROPERTY(BlueprintReadOnly) bool bSeen = false;
};

USTRUCT(BlueprintType)
struct FDCRoom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EDCRoomType Type = EDCRoomType::Office;
	UPROPERTY(BlueprintReadOnly) EDCRoomState State = EDCRoomState::Locked;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;             // built level (0 = never built). While Building, the level being built is Level+1
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;         // short
	UPROPERTY(BlueprintReadOnly) FName Icon;
	UPROPERTY(BlueprintReadOnly) TArray<FString> LevelEffects; // one SHORT line per level 1..3
	UPROPERTY(BlueprintReadOnly) int32 NextCost = 0;
	UPROPERTY(BlueprintReadOnly) float NextBuildTime = 0.f;   // run seconds
	UPROPERTY(BlueprintReadOnly) int32 RequiredSuccesses = 0;
	UPROPERTY(BlueprintReadOnly) bool bCanAct = false;
	UPROPERTY(BlueprintReadOnly) EDCPlanBlock Block = EDCPlanBlock::None; // NoCash / Locked (successes) / None
	UPROPERTY(BlueprintReadOnly) bool bMaxLevel = false;
	UPROPERTY(BlueprintReadOnly) double BuildStartedAt = 0.0; // valid while Building
	UPROPERTY(BlueprintReadOnly) double BuildEndsAt = 0.0;
	UPROPERTY(BlueprintReadOnly) int32 UpkeepPerDay = 0;
	/** Build-grid cell (DCSite::Grid). Set when the first build starts; the Office is always at (0,0). */
	UPROPERTY(BlueprintReadOnly) FIntPoint Cell = FIntPoint(-999, -999);
	UPROPERTY(BlueprintReadOnly) bool bPlaced = false;
};

USTRUCT(BlueprintType)
struct FDCRunSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) bool bWon = false;
	UPROPERTY(BlueprintReadOnly) FString EndReason;           // short
	UPROPERTY(BlueprintReadOnly) int32 DaysSurvived = 0;
	UPROPERTY(BlueprintReadOnly) double PlayTime = 0.0;       // run seconds
	UPROPERTY(BlueprintReadOnly) int32 MissionsSucceeded = 0;
	UPROPERTY(BlueprintReadOnly) int32 MissionsAttempted = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalTake = 0;
	UPROPERTY(BlueprintReadOnly) FString BestMissionName;
	UPROPERTY(BlueprintReadOnly) int32 BestMissionTake = 0;
	UPROPERTY(BlueprintReadOnly) int32 RoomsBuilt = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Crew;        // surviving crew display names
	UPROPERTY(BlueprintReadOnly) TArray<FString> Lost;        // arrested
	UPROPERTY(BlueprintReadOnly) TArray<FName> Perks;
};

/** One of the 3 save slots (UDCRunSubsystem::GetSaveSlots). Finished runs stay saved (bFinished) and load read-only. */
USTRUCT(BlueprintType)
struct FDCSaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Slot = 0;              // 0..NumSaveSlots-1
	UPROPERTY(BlueprintReadOnly) bool bUsed = false;
	UPROPERTY(BlueprintReadOnly) bool bFinished = false;      // game over or won
	UPROPERTY(BlueprintReadOnly) bool bWon = false;
	UPROPERTY(BlueprintReadOnly) FString EndReason;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) int32 Day = 1;
	UPROPERTY(BlueprintReadOnly) float Hour = 7.f;
	UPROPERTY(BlueprintReadOnly) double PlayTime = 0.0;       // run seconds
	UPROPERTY(BlueprintReadOnly) int32 Cash = 0;
	UPROPERTY(BlueprintReadOnly) int32 MissionsSucceeded = 0;
	UPROPERTY(BlueprintReadOnly) int32 RosterCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 RoomsBuilt = 0;
	UPROPERTY(BlueprintReadOnly) FDateTime SavedAt;           // UTC wall clock
};

/** A finished run on the persistent leaderboard (UDCRunSubsystem::GetLeaderboard). */
USTRUCT(BlueprintType)
struct FDCRunRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) bool bWon = false;
	UPROPERTY(BlueprintReadOnly) FString EndReason;
	UPROPERTY(BlueprintReadOnly) double PlayTime = 0.0;       // run seconds (the "run time")
	UPROPERTY(BlueprintReadOnly) int32 DaysSurvived = 0;
	UPROPERTY(BlueprintReadOnly) int32 MissionsSucceeded = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalTake = 0;
	UPROPERTY(BlueprintReadOnly) int32 RoomsBuilt = 0;
	UPROPERTY(BlueprintReadOnly) FDateTime EndedAt;           // UTC wall clock
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers. Icon names are Lucide (Content/Fonts/lucide_codepoints.json).
// ─────────────────────────────────────────────────────────────────────────────

namespace DC
{
	inline const TCHAR* TypeName(EDCCriminalType T)
	{
		switch (T)
		{
		case EDCCriminalType::Wheelman:    return TEXT("Wheelman");
		case EDCCriminalType::Muscle:      return TEXT("Muscle");
		case EDCCriminalType::Hacker:      return TEXT("Hacker");
		case EDCCriminalType::Ghost:       return TEXT("Ghost");
		case EDCCriminalType::Face:        return TEXT("Face");
		case EDCCriminalType::Safecracker: return TEXT("Safecracker");
		default: return TEXT("?");
		}
	}
	inline FName TypeIcon(EDCCriminalType T)
	{
		switch (T)
		{
		case EDCCriminalType::Wheelman:    return TEXT("car-front");
		case EDCCriminalType::Muscle:      return TEXT("dumbbell");
		case EDCCriminalType::Hacker:      return TEXT("terminal");
		case EDCCriminalType::Ghost:       return TEXT("ghost");
		case EDCCriminalType::Face:        return TEXT("venetian-mask");
		case EDCCriminalType::Safecracker: return TEXT("lock-keyhole-open");
		default: return TEXT("user");
		}
	}
	inline const TCHAR* ThreatName(EDCThreat T)
	{
		switch (T)
		{
		case EDCThreat::Guards:    return TEXT("Guards");
		case EDCThreat::Cameras:   return TEXT("Cameras");
		case EDCThreat::Alarms:    return TEXT("Alarms");
		case EDCThreat::Locks:     return TEXT("Locks");
		case EDCThreat::Vault:     return TEXT("Vault");
		case EDCThreat::Patrols:   return TEXT("Patrols");
		case EDCThreat::Witnesses: return TEXT("Witnesses");
		case EDCThreat::Pursuit:   return TEXT("Pursuit");
		default: return TEXT("?");
		}
	}
	inline FName ThreatIcon(EDCThreat T)
	{
		switch (T)
		{
		case EDCThreat::Guards:    return TEXT("shield-alert");
		case EDCThreat::Cameras:   return TEXT("cctv");
		case EDCThreat::Alarms:    return TEXT("siren");
		case EDCThreat::Locks:     return TEXT("lock");
		case EDCThreat::Vault:     return TEXT("vault");
		case EDCThreat::Patrols:   return TEXT("radar");
		case EDCThreat::Witnesses: return TEXT("eye");
		case EDCThreat::Pursuit:   return TEXT("route");
		default: return TEXT("circle-help");
		}
	}
	inline const TCHAR* QualityName(EDCQuality Q)
	{
		switch (Q)
		{
		case EDCQuality::Street: return TEXT("Street");
		case EDCQuality::Pro:    return TEXT("Pro");
		case EDCQuality::Elite:  return TEXT("Elite");
		default: return TEXT("?");
		}
	}
	inline FLinearColor QualityColor(EDCQuality Q)
	{
		switch (Q)
		{
		case EDCQuality::Street: return FLinearColor(0.55f, 0.58f, 0.62f); // steel grey
		case EDCQuality::Pro:    return FLinearColor(0.20f, 0.55f, 1.00f); // blue
		case EDCQuality::Elite:  return FLinearColor(1.00f, 0.62f, 0.10f); // amber
		default: return FLinearColor::White;
		}
	}
	inline const TCHAR* SlotName(EDCGearSlot S)
	{
		switch (S)
		{
		case EDCGearSlot::Tool:   return TEXT("Tool");
		case EDCGearSlot::Outfit: return TEXT("Outfit");
		case EDCGearSlot::Gadget: return TEXT("Gadget");
		default: return TEXT("?");
		}
	}
	inline FName SlotIcon(EDCGearSlot S)
	{
		switch (S)
		{
		case EDCGearSlot::Tool:   return TEXT("wrench");
		case EDCGearSlot::Outfit: return TEXT("shirt");
		case EDCGearSlot::Gadget: return TEXT("smartphone");
		default: return TEXT("circle-help");
		}
	}
	inline const TCHAR* RoomName(EDCRoomType R)
	{
		switch (R)
		{
		case EDCRoomType::Office:       return TEXT("Office");
		case EDCRoomType::Lounge:       return TEXT("Bunkroom");
		case EDCRoomType::Workshop:     return TEXT("Workshop");
		case EDCRoomType::Garage:       return TEXT("Kit Room");
		case EDCRoomType::OpsRoom:      return TEXT("Ops Room");
		case EDCRoomType::Clinic:       return TEXT("Clinic");
		case EDCRoomType::ServerRoom:   return TEXT("Server Room");
		case EDCRoomType::LawyerOffice: return TEXT("Lawyer");
		default: return TEXT("?");
		}
	}
	inline FName RoomIcon(EDCRoomType R)
	{
		switch (R)
		{
		case EDCRoomType::Office:       return TEXT("briefcase");
		case EDCRoomType::Lounge:       return TEXT("bed-double");
		case EDCRoomType::Workshop:     return TEXT("hammer");
		case EDCRoomType::Garage:       return TEXT("backpack");
		case EDCRoomType::OpsRoom:      return TEXT("monitor");
		case EDCRoomType::Clinic:       return TEXT("cross");
		case EDCRoomType::ServerRoom:   return TEXT("server");
		case EDCRoomType::LawyerOffice: return TEXT("scale");
		default: return TEXT("circle-help");
		}
	}
	inline const TCHAR* OutcomeName(EDCMissionOutcome O)
	{
		switch (O)
		{
		case EDCMissionOutcome::Success: return TEXT("SUCCESS");
		case EDCMissionOutcome::Failed:  return TEXT("FAILED");
		case EDCMissionOutcome::Busted:  return TEXT("BUSTED");
		default: return TEXT("");
		}
	}
	/** Per-type accent colour (linear). Used by the UI and pawns. Muted, modern. */
	inline FLinearColor TypeColor(EDCCriminalType T)
	{
		switch (T)
		{
		case EDCCriminalType::Wheelman:    return FLinearColor(1.00f, 0.42f, 0.12f); // orange
		case EDCCriminalType::Muscle:      return FLinearColor(0.90f, 0.16f, 0.16f); // red
		case EDCCriminalType::Hacker:      return FLinearColor(0.15f, 0.85f, 0.45f); // green
		case EDCCriminalType::Ghost:       return FLinearColor(0.60f, 0.62f, 0.95f); // lavender
		case EDCCriminalType::Face:        return FLinearColor(0.95f, 0.30f, 0.65f); // pink
		case EDCCriminalType::Safecracker: return FLinearColor(0.95f, 0.78f, 0.25f); // gold
		default: return FLinearColor::White;
		}
	}
	/** "1:05", "12:30", "1:02:00" for a duration in seconds. */
	inline FString Clock(double Seconds)
	{
		const int64 S = FMath::Max<int64>(0, (int64)FMath::CeilToDouble(Seconds));
		if (S >= 3600) { return FString::Printf(TEXT("%lld:%02lld:%02lld"), S / 3600, (S / 60) % 60, S % 60); }
		return FString::Printf(TEXT("%lld:%02lld"), S / 60, S % 60);
	}
}
