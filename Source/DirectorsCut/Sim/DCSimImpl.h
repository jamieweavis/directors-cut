// Director's Cut — Sim internals (Agent A). All gameplay logic of UDCRunSubsystem lives in FDCSimImpl.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCRunSubsystem.h"
#include "Sim/DCSimData.h"

/** One active modifier and where it came from (room or perk). */
struct FDCActiveMod
{
	FDCModifier Mod;
	FString Source;
	bool bFromPerk = false;
};

/** The generic modifier stack: every modifier from built rooms (their current level) and taken perks. */
struct FDCModStack
{
	TArray<FDCActiveMod> Mods;

	bool Has(EDCModKind K) const;
	/** max(Base, values of K) */
	float Max(EDCModKind K, float Base) const;
	/** sum of values of K */
	float Sum(EDCModKind K) const;
	/** product of values of K (ignores conditional entries) */
	float Product(EDCModKind K) const;
};

namespace DCSimFormula
{
	inline float FairCut(int32 Loyalty, float Base, float LoyaltyDiv, float Bonus) { return Base + (Loyalty - 50) / LoyaltyDiv + Bonus; }
	inline float Willingness(float Cut, float Fair, float Slope, float Min, float Floor)
	{
		const float W = FMath::Clamp(1.f - Slope * FMath::Max(0.f, Cut - Fair), Min, 1.f);
		return FMath::Max(W, FMath::Min(Floor, 1.f));
	}
	inline float SkillChance(float Margin, float Base, float Slope, float Min, float Max) { return FMath::Clamp(Base + Slope * Margin, Min, Max); }
	inline float Difficulty(float Base, int32 Heists, int32 Tier, float PerHeist, float PerTier) { return Base * (1.f + PerHeist * Heists) * (1.f + PerTier * Tier); }
	inline int32 XPToNext(int32 Level, float Base, float Exp) { return FMath::Max(1, FMath::RoundToInt(Base * FMath::Pow((float)Level, Exp))); }
}

struct FDCSimImpl
{
	using S_t = UDCRunSubsystem;

	static const FDCSimData& D(const S_t& S) { return *S.Data; }
	static const FDCBalance& B(const S_t& S) { return S.Data->Balance; }
	static void Changed(S_t& S) { S.OnRunStateChanged.Broadcast(); }
	static void SetPhase(S_t& S, EDCGamePhase P);

	// ── Modifiers ──
	static FDCModStack BuildMods(const S_t& S);
	static int32 RosterCap(const S_t& S, const FDCModStack& M);
	static int32 CrewsPerNight(const S_t& S, const FDCModStack& M);
	static int32 BoardSize(const S_t& S, const FDCModStack& M);
	static int32 GearSlots(const S_t& S, const FDCModStack& M);
	static int32 GearTier(const FDCModStack& M);
	static int32 GearPrice(const FDCGearDef& G, const FDCModStack& M);
	static float PayoutMult(const FDCJob& J, const FDCModStack& M);
	static int32 RetakesPerNight(const FDCModStack& M);
	static bool IsFinalCutUnlocked(const S_t& S, const FDCModStack& M);
	static int32 UnlockedTier(const S_t& S);
	static int32 Upkeep(const S_t& S, const FDCModStack& M);

	// ── Stats ──
	static FDCStatBlock EffStats(const S_t& S, const FDCModStack& M, const FDCCriminal& C);
	static int32 EffLuck(const S_t& S, const FDCModStack& M, const FDCCriminal& C);

	// ── Generation ──
	static FDCCriminal MakeCriminal(S_t& S, EDCCriminalType Type, int32 Tier, int32 BonusLevels);
	static void GenerateStartingRoster(S_t& S);
	static void GenerateRecruits(S_t& S);
	static void GenerateBoard(S_t& S);
	static void TopUpBoard(S_t& S);
	static FDCJob MakeJob(S_t& S, const FDCJobDef& Def, int32 BoardTier);
	static bool IsDoable(const FDCJobDef& Def, const TArray<EDCCriminalType>& Pool);
	static TArray<const FDCJobDef*> EligibleTemplates(const S_t& S, const FDCModStack& M);
	static int32 RoundTo(float V, int32 Step) { return FMath::Max(0, FMath::RoundToInt(V / Step) * Step); }

	// ── Resolver ──
	static FDCSuccessBreakdown Breakdown(const S_t& S, const FDCCrewPlan& Plan, bool bCheckAvailability);
	static void ResolveHeist(S_t& S, const FDCCrewPlan& Plan, const FDCSuccessBreakdown& Bd, const FDCJob& Job, FRandomStream& R, FDCHeistResult& Out);
	static EDCHeistOutcome OutcomeForRoll(float Roll, float Success, float Clean, float BustedBand);

	// ── Levelling ──
	static void AddXP(S_t& S, FDCCriminal& C, int32 XP, FDCCrewMemberResult* Out);
	static void ApplyLevelUp(S_t& S, FDCCriminal& C);

	// ── Progression ──
	static void CheckProgression(S_t& S, TArray<FString>& Events);
	static bool ComputeGameOver(const S_t& S, FString& OutReason);
	static void RefreshStatuses(S_t& S);
	static FString Fill(const FString& Tmpl, const FString& Name, const FString& Job = FString(), int32 Tier = 0, const FString& Amount = FString());
	static FString Money(int32 Amount); // "$12,500"
	static FString PickLine(const TArray<FString>& Lines, FRandomStream& R);
};
