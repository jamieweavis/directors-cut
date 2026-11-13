// Director's Cut — Sim data definitions + JSON loader (Agent A).
// Mirrors Content/Data/schema.md. Every balance constant has a hard-coded default matching schema.md.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCTypes.h"

enum class EDCModKind : uint8
{
	RosterCap, CrewsPerNight, JobBoardSize, IdleLoyalty, GearSlots, GearTier, GearDiscount,
	StatBonus, LuckBonus, ChanceBonus, PayoutMult, UpkeepMult, ArrestMult, InjuryMult, InjuryHealRate,
	FairCutBonus, MinWillingness, RecruitLevelBonus, PreventFirstArrest, Retakes, UnlockHeavyJobs,
	UnlockFinalCut, Intel, PatchUp, Bail, Dailies, Overtime,
	Unknown
};

struct FDCModifier
{
	EDCModKind Kind = EDCModKind::Unknown;
	FString KindName;
	float Value = 0.f;
	bool bHasStat = false;
	EDCStat Stat = EDCStat::Muscle;
	bool bHasType = false;
	EDCCriminalType Type = EDCCriminalType::Muscle;
	FString Condition; // ChanceBonus / PayoutMult condition, "" = always
};

struct FDCTypeDef
{
	EDCCriminalType Type = EDCCriminalType::Muscle;
	FString DisplayName;
	FString Fantasy;
	EDCStat Primary = EDCStat::Muscle;
	EDCStat Secondary = EDCStat::Driving;
	FIntPoint PrimaryRange = FIntPoint(45, 65);
	FIntPoint SecondaryRange = FIntPoint(30, 45);
	FIntPoint OtherRange = FIntPoint(10, 30);
	FIntPoint LuckRange = FIntPoint(0, 3);
	FIntPoint LoyaltyRange = FIntPoint(35, 65);
	int32 HireCostBase = 3500;
	FString Props;
};

struct FDCNamesDef
{
	TArray<FString> First;
	TArray<FString> Last;
	TArray<FString> Nicknames;
	TMap<EDCCriminalType, TArray<FString>> TypeNicknames;
};

struct FDCJobDef
{
	FName Id;
	FString DisplayName;
	int32 Tier = 1;
	TArray<EDCCriminalType> RequiredRoles;
	TArray<EDCCriminalType> BonusRoles;
	int32 MinCrew = 1;
	int32 MaxCrew = 2;
	TMap<EDCStat, float> StatWeights;
	float BaseDifficulty = 50.f;
	FIntPoint PayoutRange = FIntPoint(4000, 7000);
	int32 XPReward = 40;
	float ArrestRisk = 0.15f;
	bool bHeavy = false;
	bool bFinalCut = false;
	TArray<FString> Flavour;
};

struct FDCGearDef
{
	FDCGearItem Item;          // Price = base price
	bool bHasAffinity = false;
	EDCCriminalType Affinity = EDCCriminalType::Muscle;
};

struct FDCRoomLevelDef
{
	FString EffectText;
	FString Flavour;
	TArray<FDCModifier> Modifiers;
};

struct FDCRoomDef
{
	EDCRoomType Type = EDCRoomType::Office;
	FString DisplayName;
	FString Priority;
	FString Description;
	int32 BuildCost = 0;
	TArray<int32> UpgradeCosts; // [L1->L2, L2->L3]
	int32 RequiredHeists = 0;
	int32 UpkeepPerNight = 200;
	TArray<FDCRoomLevelDef> Levels; // 3 entries
};

struct FDCPerkDef
{
	FDCPerk Perk;
	TArray<FDCModifier> Modifiers;
};

struct FDCBalance
{
	int32 StartingCash = 25000;
	int32 StartingRoster = 4;
	int32 StartingBoard = 4;
	int32 RecruitBoardSize = 3;
	int32 RecruitRefreshNights = 3;
	float CutMin = 0.10f, CutMax = 0.90f, CutStep = 0.05f, DefaultCut = 0.30f;
	float SupportWeight = 0.25f;
	float DifficultyPerHeist = 0.10f, DifficultyPerTier = 0.05f;
	float SkillBase = 0.55f, SkillSlope = 0.75f, SkillMin = 0.05f, SkillMax = 0.95f;
	float RoleFitPer = 0.03f, RoleFitCap = 0.09f;
	float FairCutBase = 0.30f, FairCutLoyaltyDiv = 500.f;
	float WillSlope = 2.0f, WillMin = 0.2f, RefusalThreshold = 0.35f;
	float WillFactorBase = 0.60f, WillFactorScale = 0.40f;
	float GenerosityBonus = 0.04f;
	float SuccessMin = 0.02f, SuccessMax = 0.97f;
	float LuckyBreakPerLuck = 0.02f;
	float CleanBand = 0.6f, BustedBand = 0.10f;
	float MessyInjuryChance = 0.25f; FIntPoint MessyInjuryNights = FIntPoint(1, 2);
	float FailedInjuryChance = 0.5f; FIntPoint FailedInjuryNights = FIntPoint(1, 3);
	float BustedArrestMult = 2.0f;
	float XPMultClean = 1.0f, XPMultMessy = 0.7f, XPMultFailed = 0.25f, XPMultBusted = 0.25f;
	float LevelCurveBase = 100.f, LevelCurveExp = 1.4f; int32 MaxLevel = 10;
	int32 LevelUpPrimary = 4, LevelUpSecondary = 2, LevelUpOther = 1, LuckEveryNLevels = 2;
	int32 LoyaltySuccessFair = 5, LoyaltyGreedy = -8, LoyaltyInjury = -3, LoyaltyMin = 0, LoyaltyMax = 100;
	TArray<int32> TierUnlockHeists = { 0, 3, 7, 12 };
	float TierPayoutMult = 1.6f;
	int32 UpkeepPerCriminal = 300, UpkeepPerHeistBlock = 1000, UpkeepHeistBlock = 4;
	float ArrestRiskPer5Heists = 0.02f;
	float RecruitCostTierMult = 1.4f; int32 RecruitStatBonusPerTier = 5;
	int32 PerkEveryNHeists = 3, PerkOfferSize = 3;
	int32 BaseRosterCap = 4, BaseCrewsPerNight = 1, BaseGearSlots = 2, BaseJobBoard = 3;
	int32 FinalCutRequiredHeists = 12;
};

struct FDCStringsDef
{
	TMap<EDCHeistOutcome, FString> Stamps;
	TMap<EDCHeistOutcome, TArray<FString>> OutcomeLines;
	TArray<FString> LuckyBreak;
	FString GameOverBroke = TEXT("Couldn't make payroll.");
	FString GameOverNoCrew = TEXT("Nobody left to cast.");
	FString WinReason = TEXT("The Premiere was a smash.");
	FString TierUnlocked = TEXT("Tier {tier} productions unlocked!");
	FString RecruitsRefreshed = TEXT("New faces at the desk.");
	TMap<FString, FString> NightEvents; // every nightEvents key (defaults filled for the ones the Sim uses)
	FString Event(const TCHAR* Key) const { const FString* V = NightEvents.Find(Key); return V ? *V : FString(); }
};

struct FDCSimData
{
	FDCBalance Balance;
	TArray<FDCTypeDef> Types;   // always 6 after load (missing entries filled with built-in defaults)
	FDCNamesDef Names;
	TArray<FDCJobDef> Jobs;
	TArray<FDCGearDef> Gear;
	TArray<FDCRoomDef> Rooms;   // always 8 after load (missing rooms filled with built-in defaults)
	TArray<FDCPerkDef> Perks;
	FDCStringsDef Strings;
	TArray<FString> LoadWarnings;

	const FDCTypeDef& GetType(EDCCriminalType T) const;
	const FDCRoomDef& GetRoom(EDCRoomType T) const;
	const FDCJobDef* FindJob(FName Id) const;
	const FDCGearDef* FindGear(FName Id) const;
	const FDCPerkDef* FindPerk(FName Id) const;

	/** Loads every data file. Looks in DataDir (default ProjectContentDir/Data), falls back to the test fixtures dir. */
	static TSharedRef<FDCSimData> Load(const FString& DataDirOverride = FString());
	static FString DefaultDataDir();
	static FString FixturesDir();
};

namespace DCSim
{
	bool ParseType(const FString& S, EDCCriminalType& Out);
	bool ParseStat(const FString& S, EDCStat& Out);
	bool ParseSlot(const FString& S, EDCGearSlot& Out);
	bool ParseRoom(const FString& S, EDCRoomType& Out);
	EDCModKind ParseModKind(const FString& S);
}
