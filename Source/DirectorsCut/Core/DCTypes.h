// Director's Cut — shared data contracts (LEAD-OWNED, frozen during Wave 1).
// Agents: do not edit. Request changes in your final report.
#pragma once

#include "CoreMinimal.h"
#include "DCTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

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

UENUM(BlueprintType)
enum class EDCStat : uint8
{
	Driving,
	Muscle,
	Tech,
	Stealth,
	Charm,
	Finesse,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCGearSlot : uint8
{
	Tool,
	Outfit,
	Gadget,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCCriminalStatus : uint8
{
	Available,
	OnJob,      // committed to a plan for tonight
	Injured,    // InjuredNights > 0
	Arrested    // gone for good (unless bailed by the Lawyer)
};

UENUM(BlueprintType)
enum class EDCHeistOutcome : uint8
{
	None,
	Clean,   // "IT'S A WRAP"
	Messy,   // "NEEDS FIXING IN POST"
	Failed,  // "CUT!"
	Busted   // "SHUT DOWN"
};

UENUM(BlueprintType)
enum class EDCGamePhase : uint8
{
	Title,     // no run yet
	Planning,  // in the hideout, planning
	Dailies,   // EndNight() just resolved; UI is revealing results
	GameOver,
	Won
};

UENUM(BlueprintType)
enum class EDCRoomType : uint8
{
	Office,
	Lounge,
	Workshop,
	Garage,
	ScreeningRoom,
	Clinic,
	ServerCloset,
	LawyerOffice,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCRoomState : uint8
{
	Locked,    // dark silhouette, dust sheets, tape
	Building,  // transient: scaffolding during the reveal
	Built      // lit, Level 1..3
};

UENUM(BlueprintType)
enum class EDCStationType : uint8
{
	DirectorDesk,
	JobBoard,
	Lounge,
	Workbench,
	GarageDoor,
	Projector,
	BlueprintTable,
	ScreeningRoom,
	Clinic,
	ServerCloset,
	Lawyer
};

// Panels the UI can open. World (stations) calls UDCUISubsystem::OpenPanel.
UENUM(BlueprintType)
enum class EDCPanel : uint8
{
	None,
	Title,
	JobBoard,       // job cards -> pick one -> CrewPlanner
	CrewPlanner,
	Workbench,      // gear shop + loadouts
	Roster,         // lounge
	Desk,           // finances + recruits
	Blueprint,      // build/upgrade rooms
	PerkDraft,
	Dailies,        // night resolution sequence
	GameOver,
	Credits,        // win
	Settings,       // projector
	ScreeningRoom,  // retakes / Final Cut info
	Clinic,
	Lawyer,
	ServerCloset
};

UENUM(BlueprintType)
enum class EDCMusicState : uint8
{
	None,
	Title,
	Planning,
	Dailies,
	Win,
	GameOver
};

UENUM(BlueprintType)
enum class EDCVolumeChannel : uint8
{
	Master,
	Music,
	SFX,
	Ambience
};

// ─────────────────────────────────────────────────────────────────────────────
// Structs
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FDCStatBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Driving = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Muscle = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Tech = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Stealth = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Charm = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Finesse = 0;

	int32 Get(EDCStat S) const
	{
		switch (S)
		{
		case EDCStat::Driving: return Driving;
		case EDCStat::Muscle:  return Muscle;
		case EDCStat::Tech:    return Tech;
		case EDCStat::Stealth: return Stealth;
		case EDCStat::Charm:   return Charm;
		case EDCStat::Finesse: return Finesse;
		default: return 0;
		}
	}
	int32& GetMutable(EDCStat S)
	{
		switch (S)
		{
		case EDCStat::Driving: return Driving;
		case EDCStat::Muscle:  return Muscle;
		case EDCStat::Tech:    return Tech;
		case EDCStat::Stealth: return Stealth;
		case EDCStat::Charm:   return Charm;
		default:               return Finesse;
		}
	}
	void Set(EDCStat S, int32 V) { GetMutable(S) = V; }
	FDCStatBlock& operator+=(const FDCStatBlock& O)
	{
		for (int32 i = 0; i < (int32)EDCStat::MAX; ++i) { GetMutable((EDCStat)i) += O.Get((EDCStat)i); }
		return *this;
	}
	FDCStatBlock operator+(const FDCStatBlock& O) const { FDCStatBlock R = *this; R += O; return R; }
};

USTRUCT(BlueprintType)
struct FDCCriminal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString FirstName;
	UPROPERTY(BlueprintReadOnly) FString Nickname;
	UPROPERTY(BlueprintReadOnly) FString LastName;
	UPROPERTY(BlueprintReadOnly) EDCCriminalType Type = EDCCriminalType::Muscle;

	UPROPERTY(BlueprintReadOnly) FDCStatBlock BaseStats;     // includes level-up gains
	UPROPERTY(BlueprintReadOnly) int32 Level = 1;             // 1..10
	UPROPERTY(BlueprintReadOnly) int32 XP = 0;                // progress toward next level
	UPROPERTY(BlueprintReadOnly) int32 XPToNext = 100;
	UPROPERTY(BlueprintReadOnly) int32 Luck = 0;              // 0..10 (base, excl. gear)
	UPROPERTY(BlueprintReadOnly) int32 Loyalty = 50;          // 0..100

	UPROPERTY(BlueprintReadOnly) EDCCriminalStatus Status = EDCCriminalStatus::Available;
	UPROPERTY(BlueprintReadOnly) int32 InjuredNights = 0;

	// Gear item id per slot (NAME_None = empty). Keys are EDCGearSlot.
	UPROPERTY(BlueprintReadOnly) TMap<EDCGearSlot, FName> Equipped;

	UPROPERTY(BlueprintReadOnly) int32 HireCost = 0;          // only meaningful for recruits
	UPROPERTY(BlueprintReadOnly) int32 HeistsPulled = 0;

	FString GetDisplayName() const { return FString::Printf(TEXT("%s \"%s\" %s"), *FirstName, *Nickname, *LastName); }
	FString GetShortName() const { return FirstName; }
	bool IsAvailable() const { return Status == EDCCriminalStatus::Available; }
};

USTRUCT(BlueprintType)
struct FDCGearItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) EDCGearSlot Slot = EDCGearSlot::Tool;
	UPROPERTY(BlueprintReadOnly) int32 Price = 0;             // after discounts
	UPROPERTY(BlueprintReadOnly) FDCStatBlock StatBonus;      // can be negative
	UPROPERTY(BlueprintReadOnly) int32 LuckBonus = 0;
	UPROPERTY(BlueprintReadOnly) float ArrestRiskMultiplier = 1.f; // 0.8 = -20% arrest risk
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;               // 1 = fence crate (office), 2 = workshop, 3 = workshop L2
	UPROPERTY(BlueprintReadOnly) bool bAvailable = true;        // purchasable right now (room requirements met)
};

USTRUCT(BlueprintType)
struct FDCJob
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;       // instance id on the board
	UPROPERTY(BlueprintReadOnly) FName TemplateId;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Flavour;             // the "script"
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;
	UPROPERTY(BlueprintReadOnly) TArray<EDCCriminalType> RequiredRoles;
	UPROPERTY(BlueprintReadOnly) TArray<EDCCriminalType> BonusRoles; // matching these beyond required gives RoleFitBonus
	UPROPERTY(BlueprintReadOnly) int32 MinCrew = 1;
	UPROPERTY(BlueprintReadOnly) int32 MaxCrew = 2;
	UPROPERTY(BlueprintReadOnly) TMap<EDCStat, float> StatWeights; // sums to 1
	UPROPERTY(BlueprintReadOnly) float Difficulty = 50.f;      // already scaled for heists completed & tier
	UPROPERTY(BlueprintReadOnly) int32 Payout = 0;             // rolled total payout
	UPROPERTY(BlueprintReadOnly) int32 XPReward = 50;
	UPROPERTY(BlueprintReadOnly) float ArrestRisk = 0.2f;      // base, 0..1
	UPROPERTY(BlueprintReadOnly) bool bHeavy = false;          // requires Garage
	UPROPERTY(BlueprintReadOnly) bool bFinalCut = false;       // "The Premiere" — completing it wins
	UPROPERTY(BlueprintReadOnly) bool bCommitted = false;      // a plan is committed for this job tonight
};

USTRUCT(BlueprintType)
struct FDCCrewPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) int32 JobId = INDEX_NONE;
	UPROPERTY(BlueprintReadWrite) TArray<int32> CriminalIds;
	UPROPERTY(BlueprintReadWrite) float Cut = 0.3f;            // 0.10..0.90, steps of 0.05
};

// One line of the "Why this %" breakdown.
USTRUCT(BlueprintType)
struct FDCBreakdownLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Label;   // e.g. "Skill vs difficulty"
	UPROPERTY(BlueprintReadOnly) FString Detail;  // e.g. "Crew 62 vs Difficulty 55 (+13% margin)"
	UPROPERTY(BlueprintReadOnly) float Value = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bIsMultiplier = false; // Value is a factor (x0.86) rather than additive (+0.04)
	UPROPERTY(BlueprintReadOnly) bool bIsWarning = false;    // render red
};

USTRUCT(BlueprintType)
struct FDCSuccessBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bValid = false;          // plan can be committed
	UPROPERTY(BlueprintReadOnly) FString InvalidReason;         // why not (missing role, crew size, refusal, capacity...)
	UPROPERTY(BlueprintReadOnly) TArray<EDCCriminalType> MissingRoles;
	UPROPERTY(BlueprintReadOnly) TArray<int32> RefusingCriminalIds;   // willingness < refusal threshold
	UPROPERTY(BlueprintReadOnly) TMap<int32, float> Willingness;      // per criminal id
	UPROPERTY(BlueprintReadOnly) TMap<int32, float> FairCut;          // per criminal id
	UPROPERTY(BlueprintReadOnly) TMap<EDCStat, float> CrewStats;      // CrewStat(s)

	UPROPERTY(BlueprintReadOnly) float CrewScore = 0.f;
	UPROPERTY(BlueprintReadOnly) float Difficulty = 0.f;
	UPROPERTY(BlueprintReadOnly) float Margin = 0.f;
	UPROPERTY(BlueprintReadOnly) float SkillChance = 0.f;
	UPROPERTY(BlueprintReadOnly) float RoleFitBonus = 0.f;
	UPROPERTY(BlueprintReadOnly) float GenerosityBonus = 0.f;
	UPROPERTY(BlueprintReadOnly) float ModifierBonus = 0.f;     // rooms + perks, summed additive chance
	UPROPERTY(BlueprintReadOnly) float CrewWill = 1.f;
	UPROPERTY(BlueprintReadOnly) float WillFactor = 1.f;
	UPROPERTY(BlueprintReadOnly) float SuccessChance = 0.f;     // final, 0.02..0.97
	UPROPERTY(BlueprintReadOnly) float CleanThreshold = 0.f;    // roll < this => Clean
	UPROPERTY(BlueprintReadOnly) float LuckyBreakChance = 0.f;
	UPROPERTY(BlueprintReadOnly) float ArrestChanceOnFail = 0.f; // per member, after gear/rooms/perks
	UPROPERTY(BlueprintReadOnly) int32 Payout = 0;               // expected total payout (after modifiers)
	UPROPERTY(BlueprintReadOnly) int32 DirectorTake = 0;         // Payout * Cut

	UPROPERTY(BlueprintReadOnly) TArray<FDCBreakdownLine> Lines; // human-readable, in order
};

USTRUCT(BlueprintType)
struct FDCCrewMemberResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 CriminalId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) bool bInjured = false;
	UPROPERTY(BlueprintReadOnly) int32 InjuryNights = 0;
	UPROPERTY(BlueprintReadOnly) bool bArrested = false;
	UPROPERTY(BlueprintReadOnly) bool bArrestPrevented = false;  // Stunt Double etc.
	UPROPERTY(BlueprintReadOnly) int32 XPGained = 0;
	UPROPERTY(BlueprintReadOnly) bool bLevelledUp = false;
	UPROPERTY(BlueprintReadOnly) int32 NewLevel = 1;
	UPROPERTY(BlueprintReadOnly) int32 LoyaltyDelta = 0;
};

USTRUCT(BlueprintType)
struct FDCHeistResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 ResultId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int32 JobId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString JobName;
	UPROPERTY(BlueprintReadOnly) FDCCrewPlan Plan;
	UPROPERTY(BlueprintReadOnly) float SuccessChance = 0.f;
	UPROPERTY(BlueprintReadOnly) float CleanThreshold = 0.f;
	UPROPERTY(BlueprintReadOnly) float Roll = 0.f;              // 0..1, the final roll used
	UPROPERTY(BlueprintReadOnly) float FirstRoll = 0.f;         // before lucky break / retake
	UPROPERTY(BlueprintReadOnly) EDCHeistOutcome Outcome = EDCHeistOutcome::None;
	UPROPERTY(BlueprintReadOnly) bool bLuckyBreak = false;      // first roll failed, free re-roll fired
	UPROPERTY(BlueprintReadOnly) bool bRetakeAvailable = false; // UI may offer UseRetake(ResultId)
	UPROPERTY(BlueprintReadOnly) bool bRetaken = false;
	UPROPERTY(BlueprintReadOnly) bool bFinalCut = false;
	UPROPERTY(BlueprintReadOnly) int32 PayoutTotal = 0;
	UPROPERTY(BlueprintReadOnly) int32 DirectorTake = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FDCCrewMemberResult> Members;
	UPROPERTY(BlueprintReadOnly) FString FlavourLine;          // e.g. "Vinnie got out clean, smelling of diamonds."
};

USTRUCT(BlueprintType)
struct FDCNightReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Night = 0;               // the night that just ended
	UPROPERTY(BlueprintReadOnly) TArray<FDCHeistResult> Results;
	UPROPERTY(BlueprintReadOnly) int32 CashBefore = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalTake = 0;
	UPROPERTY(BlueprintReadOnly) int32 UpkeepPaid = 0;
	UPROPERTY(BlueprintReadOnly) int32 CashAfter = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Events;        // "Lounge: idle crew +1 loyalty", "Tier 2 jobs unlocked!"
	UPROPERTY(BlueprintReadOnly) bool bGameOver = false;
	UPROPERTY(BlueprintReadOnly) bool bWon = false;
	UPROPERTY(BlueprintReadOnly) bool bPerkOffered = false;
};

USTRUCT(BlueprintType)
struct FDCRoom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EDCRoomType Type = EDCRoomType::Office;
	UPROPERTY(BlueprintReadOnly) EDCRoomState State = EDCRoomState::Locked;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;               // 0 when locked, 1..3 when built
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) TArray<FString> LevelEffects;  // one line per level 1..3
	UPROPERTY(BlueprintReadOnly) int32 NextCost = 0;            // build cost if locked, upgrade cost if built (<3)
	UPROPERTY(BlueprintReadOnly) int32 RequiredHeists = 0;
	UPROPERTY(BlueprintReadOnly) bool bCanAct = false;          // can build/upgrade right now (prereqs + cash)
	UPROPERTY(BlueprintReadOnly) FString BlockedReason;         // "Needs 2 heists", "Not enough cash", "Max level"
	UPROPERTY(BlueprintReadOnly) int32 UpkeepPerNight = 0;
};

USTRUCT(BlueprintType)
struct FDCPerk
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString Flavour;
};

USTRUCT(BlueprintType)
struct FDCRunSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) bool bWon = false;
	UPROPERTY(BlueprintReadOnly) FString EndReason;             // "Couldn't make payroll", "The Premiere was a smash"
	UPROPERTY(BlueprintReadOnly) int32 NightsSurvived = 0;
	UPROPERTY(BlueprintReadOnly) int32 HeistsCompleted = 0;
	UPROPERTY(BlueprintReadOnly) int32 HeistsAttempted = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalTake = 0;
	UPROPERTY(BlueprintReadOnly) FString BestHeistName;
	UPROPERTY(BlueprintReadOnly) int32 BestHeistTake = 0;
	UPROPERTY(BlueprintReadOnly) int32 RoomsBuilt = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Starring;      // surviving crew display names (+ type)
	UPROPERTY(BlueprintReadOnly) TArray<FString> InMemoriam;    // arrested crew
	UPROPERTY(BlueprintReadOnly) TArray<FString> PerksTaken;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
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
	inline const TCHAR* StatName(EDCStat S)
	{
		switch (S)
		{
		case EDCStat::Driving: return TEXT("Driving");
		case EDCStat::Muscle:  return TEXT("Muscle");
		case EDCStat::Tech:    return TEXT("Tech");
		case EDCStat::Stealth: return TEXT("Stealth");
		case EDCStat::Charm:   return TEXT("Charm");
		case EDCStat::Finesse: return TEXT("Finesse");
		default: return TEXT("?");
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
	inline const TCHAR* RoomName(EDCRoomType R)
	{
		switch (R)
		{
		case EDCRoomType::Office:        return TEXT("Director's Office");
		case EDCRoomType::Lounge:        return TEXT("The Lounge");
		case EDCRoomType::Workshop:      return TEXT("Workshop");
		case EDCRoomType::Garage:        return TEXT("Garage");
		case EDCRoomType::ScreeningRoom: return TEXT("Screening Room");
		case EDCRoomType::Clinic:        return TEXT("Back-Alley Clinic");
		case EDCRoomType::ServerCloset:  return TEXT("Server Closet");
		case EDCRoomType::LawyerOffice:  return TEXT("Lawyer's Office");
		default: return TEXT("?");
		}
	}
	inline const TCHAR* OutcomeStamp(EDCHeistOutcome O)
	{
		switch (O)
		{
		case EDCHeistOutcome::Clean:  return TEXT("IT'S A WRAP");
		case EDCHeistOutcome::Messy:  return TEXT("NEEDS FIXING IN POST");
		case EDCHeistOutcome::Failed: return TEXT("CUT!");
		case EDCHeistOutcome::Busted: return TEXT("SHUT DOWN");
		default: return TEXT("");
		}
	}
	// Per-type accent colour (linear). Used by UI and pawns.
	inline FLinearColor TypeColor(EDCCriminalType T)
	{
		switch (T)
		{
		case EDCCriminalType::Wheelman:    return FLinearColor(1.00f, 0.45f, 0.10f); // orange
		case EDCCriminalType::Muscle:      return FLinearColor(0.85f, 0.10f, 0.12f); // red
		case EDCCriminalType::Hacker:      return FLinearColor(0.10f, 0.90f, 0.45f); // green
		case EDCCriminalType::Ghost:       return FLinearColor(0.55f, 0.60f, 1.00f); // pale blue
		case EDCCriminalType::Face:        return FLinearColor(1.00f, 0.25f, 0.70f); // pink
		case EDCCriminalType::Safecracker: return FLinearColor(1.00f, 0.80f, 0.20f); // gold
		default: return FLinearColor::White;
		}
	}
}
