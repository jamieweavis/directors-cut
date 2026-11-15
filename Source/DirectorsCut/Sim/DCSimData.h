// Director's Cut — static game data loaded from Content/Data/*.json (schema: Content/Data/schema.md). Agent A.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCTypes.h"

/** One effect from the shared vocabulary (schema.md "Effects vocabulary"). */
struct FDCEffect
{
	FName Kind;
	float Value = 0.f;
	FString Param;
	float Threshold = 0.f;
};

struct FDCTraitDef
{
	FDCTrait Trait;
	TArray<FDCEffect> Effects;
};

struct FDCPerkDef
{
	FDCPerk Perk;
	TArray<FDCEffect> Effects;
};

struct FDCGearDef
{
	FDCGearItem Item;  // Price = base price
	bool bLoot = false;
};

struct FDCTypeDef
{
	EDCCriminalType Type = EDCCriminalType::Muscle;
	TArray<FName> Abilities;
	TArray<FName> TraitPool;
};

struct FDCMissionTemplate
{
	FName Id;
	FString DisplayName;
	FName Icon;
	TArray<FString> Flavour;
	int32 Tier = 1;
	FIntPoint CrewSize = FIntPoint(1, 1);
	FVector2D DurationSec = FVector2D(60, 90);
	TArray<EDCThreat> FixedThreats;
	TArray<EDCThreat> PoolThreats;
	FIntPoint Extra = FIntPoint(0, 0);
	FIntPoint Cost = FIntPoint(0, 0);
	FIntPoint Payout = FIntPoint(1000, 1000);
	int32 XPReward = 50;
	float ArrestRisk = 0.f;
	float NightChance = 0.f;
	float Weight = 1.f;
	bool bFinal = false;
};

struct FDCRoomDef
{
	EDCRoomType Type = EDCRoomType::Office;
	FString DisplayName;
	FString Description;
	FName Icon;
	TArray<int32> Cost;              // per level 1..3
	TArray<float> BuildSec;
	TArray<int32> RequiredSuccesses;
	TArray<int32> UpkeepPerDay;
	TArray<FString> LevelEffects;
	TArray<TArray<FDCEffect>> Levels; // effects per level (not cumulative)
};

struct FDCBalance
{
	int32 StartingCash = 6000;
	int32 StartingRoster = 4;
	FIntPoint StartingRosterLevels = FIntPoint(1, 2);
	float StartHour = 7.f, DawnHour = 6.f, DuskHour = 20.f, GameMinutesPerSecond = 1.f;
	float AutosaveSeconds = 30.f;

	int32 BoardBaseSize = 4;
	float PostIntervalSec = 40.f;
	FVector2D ExpireSecRange = FVector2D(300, 600);
	TArray<TArray<float>> TierWeights;
	float RareChance = 0.08f;
	float RarePayoutMult = 1.f;   // rare postings: bigger take ...
	int32 RareLevelAdd = 0;       // ... a tougher crew to beat ...
	int32 RareExtraThreats = 0;   // ... and more in the way
	TArray<FName> StartingBoard;

	TArray<int32> UnlockSuccesses = { 0, 3, 8, 15 };
	TArray<FIntPoint> LevelRange = { FIntPoint(1, 2), FIntPoint(3, 4), FIntPoint(5, 7), FIntPoint(8, 10) };

	float ThreatPoints = 1.5f;
	float LFAtOrAbove = 1.f, LFPerLevelAbove = 0.05f, LFMaxFactor = 1.2f;
	TArray<float> LFBelow = { 0.75f, 0.5f, 0.1f };
	float MaxChance = 2.f;
	float BaseMult = 1.f; // execution risk: scales crew + counter odds (not modifiers)

	float CutMin = 0.1f, CutMax = 0.9f, CutStep = 0.05f, CutDefault = 0.3f;
	float FairCutBase = 0.3f, FairCutLoyaltyDiv = 500.f;
	float WillSlope = 3.f, WillMin = 0.2f, RefusalThreshold = 0.45f, WillFactorBase = 0.6f, WillFactorScale = 0.4f;

	float BustedBand = 0.25f, BonusPayoutMult = 1.5f, FailXpMult = 0.25f;
	float InjuryFailChance = 0.35f, InjuryBustedChance = 0.6f;
	FVector2D InjurySecondsRange = FVector2D(90, 240);

	int32 MaxLevel = 10;
	float XPBase = 100.f, XPExp = 1.4f;

	int32 LoyaltyStart = 60, LoyaltySuccessFair = 5, LoyaltyGreedy = -8, LoyaltyInjury = -3, LoyaltyMin = 0, LoyaltyMax = 100, LoyaltyQuitBelow = 10;
	int32 UpkeepPerCriminal = 300;

	int32 RecruitBoardSize = 3;
	float RecruitRefreshSec = 240.f;
	int32 RecruitRefreshCost = 400, RecruitCostBase = 1200, RecruitCostPerLevel = 500;
	TArray<float> QualityCostMult = { 1.f, 1.8f, 3.f };
	TArray<TArray<float>> QualityOdds;

	TArray<int32> QualityAbilities = { 1, 1, 2 };
	TArray<int32> QualityTraits = { 1, 2, 3 };
	TArray<float> NegativeTraitChance = { 0.35f, 0.2f, 0.1f };

	int32 GearBaseSlots = 2;
	float SellRefund = 0.5f, LootChance = 0.12f, RareLootChance = 0.6f;
	float NightMissionBonus = 0.1f;
	float MinDurationMult = 0.5f;
	int32 PerkEveryN = 3, PerkOfferSize = 3;
	FName FinalTemplateId = TEXT("the_big_score");
	int32 FinalRequiredSuccesses = 15;
	int32 CashFloor = 0;
};

struct FDCSimData
{
	FDCBalance Balance;
	TMap<FName, FDCAbility> Abilities;
	TMap<FName, FDCTraitDef> Traits;
	TMap<FName, FDCPerkDef> Perks;
	TArray<FName> PerkOrder;               // file order (deterministic draws)
	TMap<FName, FDCGearDef> Gear;
	TArray<FName> GearOrder;               // file order
	TArray<FDCTypeDef> Types;
	TArray<FDCMissionTemplate> Missions;
	TArray<FDCRoomDef> Rooms;
	TArray<FString> FirstNames, LastNames;  // FirstNames: names for anyone (body picked from the seed)
	TArray<FString> MaleFirstNames, FemaleFirstNames;
	TMap<EDCCriminalType, TArray<FString>> Nicknames;
	FString SourceDir;
	int32 Warnings = 0;

	/** Loads every file. Dir empty = Content/Data, falling back to Tests/Fixtures when Content/Data is missing or v1. */
	bool Load(const FString& DirOverride);

	const FDCTypeDef* FindType(EDCCriminalType T) const;
	const FDCMissionTemplate* FindTemplate(FName Id) const;
	const FDCRoomDef* FindRoom(EDCRoomType T) const;

	static FString FixturesDir();
	static FString ContentDataDir();
	/** True if Dir/missions.json exists and is the v2 format (has a "missions" array). */
	static bool IsV2Dir(const FString& Dir);

	static bool ParseThreat(const FString& S, EDCThreat& Out);
	static bool ParseType(const FString& S, EDCCriminalType& Out);
	static bool ParseSlot(const FString& S, EDCGearSlot& Out);
	static bool ParseRoom(const FString& S, EDCRoomType& Out);
};
