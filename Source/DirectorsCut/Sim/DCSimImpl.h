// Director's Cut — simulation internals (Agent A). Friend of UDCRunSubsystem; static helpers operate on its private state.
#pragma once

#include "CoreMinimal.h"
#include "Core/DCRunSubsystem.h"
#include "Sim/DCSimData.h"

struct FDCSimImpl
{
	using S = UDCRunSubsystem;

	// ── data / effects ──
	static const FDCBalance& Bal(const S& Sim);
	/** Calls F for every global effect (perks taken + built rooms at their current level). SrcId: perk id or "room.<Type>". */
	static void ForEachGlobalEffect(const S& Sim, TFunctionRef<void(const FDCEffect&, FName SrcId, FName Icon, const FString& Label)> F);
	static float SumGlobal(const S& Sim, FName Kind);
	static float ProdGlobal(const S& Sim, FName Kind);     // only effects without a type param
	static bool MinGlobal(const S& Sim, FName Kind, float& OutValue, float* OutThreshold = nullptr);
	static bool MaxGlobal(const S& Sim, FName Kind, float& OutValue);
	static float TraitProd(const S& Sim, int32 CriminalId, FName Kind);
	static float TraitSum(const S& Sim, int32 CriminalId, FName Kind);

	// ── crew helpers ──
	static FDCCriminal* RosterMut(S& Sim, int32 Id);
	static bool SlotEnabled(const S& Sim, EDCGearSlot Slot);
	static float FairCut(const S& Sim, const FDCCriminal& C);
	static float Willingness(const S& Sim, const FDCCriminal& C, float Cut);
	static float LevelFactor(const S& Sim, float EffLevel, int32 MissionLevel);
	static int32 XPToNext(const S& Sim, int32 Level);
	static int32 GearPrice(const S& Sim, const FDCGearDef& G);

	// ── clock ──
	static double TotalHours(const S& Sim, double T);
	static int32 DawnIndexAt(const S& Sim, double T);
	static double DawnTime(const S& Sim, int32 DawnIndex);

	// ── generation ──
	static FDCCriminal MakeCriminal(S& Sim, EDCCriminalType Type, EDCQuality Quality, int32 Level, FName ForcedAbility = NAME_None);
	static FDCMission MakeMission(S& Sim, const FDCMissionTemplate& T);
	static const FDCMissionTemplate* PickTemplate(S& Sim);
	static void PostMission(S& Sim, const FDCMissionTemplate& T);
	static void TryPostFinal(S& Sim);
	static void RegenerateRecruits(S& Sim);
	static void GenerateStartingRoster(S& Sim);
	static int32 BoardCount(const S& Sim);   // board postings (not in progress)
	static int32 BoardSize(const S& Sim);
	static double PostInterval(const S& Sim);

	// ── missions ──
	static FDCSuccessBreakdown Evaluate(const S& Sim, const FDCMission& M, const TArray<int32>& CrewIds, float Cut, bool bNight);
	static void CompleteMission(S& Sim, int32 MissionId);
	static void OfferPerks(S& Sim);

	// ── time ──
	static double NextEventTime(const S& Sim);
	static void ProcessDue(S& Sim);
	static void Payday(S& Sim);
	static void CheckBroke(S& Sim);
	static void EndRun(S& Sim, bool bWon, const FString& Reason);
};
