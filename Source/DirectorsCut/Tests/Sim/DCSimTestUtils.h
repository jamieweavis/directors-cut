// Director's Cut — shared helpers for DirectorsCut.Sim.* automation tests (Agent A).
#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Engine/GameInstance.h"
#include "Core/DCRunSubsystem.h"
#include "Sim/DCSimImpl.h"
#include "Sim/DCSimData.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Friend of UDCRunSubsystem: lets tests poke private run state. */
struct FDCSimTestAccess
{
	static void SetDataDir(UDCRunSubsystem& S, const FString& Dir) { S.DataDirOverride = Dir; }
	static int32& Cash(UDCRunSubsystem& S) { return S.Cash; }
	static int32& Heists(UDCRunSubsystem& S) { return S.HeistsCompleted; }
	static int32& NextId(UDCRunSubsystem& S) { return S.NextId; }
	static TArray<FDCCriminal>& Roster(UDCRunSubsystem& S) { return S.Roster; }
	static TArray<FDCJob>& Jobs(UDCRunSubsystem& S) { return S.Jobs; }
	static TArray<FDCPerk>& PerksTaken(UDCRunSubsystem& S) { return S.PerksTaken; }
	static TArray<FDCRoom>& Rooms(UDCRunSubsystem& S) { return S.Rooms; }
	static TMap<int32, int32>& RollSeeds(UDCRunSubsystem& S) { return S.JobRollSeeds; }
	static const FDCSimData& Data(const UDCRunSubsystem& S) { return *S.Data; }
	static EDCGamePhase& Phase(UDCRunSubsystem& S) { return S.Phase; }

	static void SetRoom(UDCRunSubsystem& S, EDCRoomType T, int32 Level)
	{
		for (FDCRoom& R : S.Rooms) { if (R.Type == T) { R.Level = Level; R.State = Level > 0 ? EDCRoomState::Built : EDCRoomState::Locked; } }
	}
	static void AddPerk(UDCRunSubsystem& S, FName Id)
	{
		if (const FDCPerkDef* P = S.Data->FindPerk(Id)) { S.PerksTaken.Add(P->Perk); }
	}
	static void Regenerate(UDCRunSubsystem& S) { FDCSimImpl::GenerateBoard(S); }

	/** Adds a hand-made criminal to the roster. All stats = Stat. */
	static FDCCriminal& AddCriminal(UDCRunSubsystem& S, EDCCriminalType Type, int32 Stat, int32 Loyalty = 50, int32 Luck = 0)
	{
		FDCCriminal C;
		C.Id = S.NextId++;
		C.FirstName = FString::Printf(TEXT("Test%d"), C.Id);
		C.Nickname = TEXT("Nick");
		C.LastName = TEXT("Tester");
		C.Type = Type;
		for (int32 i = 0; i < (int32)EDCStat::MAX; ++i) { C.BaseStats.Set((EDCStat)i, Stat); }
		C.Loyalty = Loyalty;
		C.Luck = Luck;
		C.HireCost = 3000;
		return S.Roster.Add_GetRef(C);
	}
	/** Adds a hand-made job. Weight 1.0 on Muscle. */
	static FDCJob& AddJob(UDCRunSubsystem& S, TArray<EDCCriminalType> Roles, float Difficulty, int32 MinCrew = 1, int32 MaxCrew = 4)
	{
		FDCJob J;
		J.Id = S.NextId++;
		J.TemplateId = TEXT("test_job");
		J.DisplayName = TEXT("Test Job");
		J.Tier = 1;
		J.RequiredRoles = Roles;
		J.MinCrew = MinCrew;
		J.MaxCrew = MaxCrew;
		J.StatWeights.Add(EDCStat::Muscle, 1.f);
		J.Difficulty = Difficulty;
		J.Payout = 10000;
		J.XPReward = 50;
		J.ArrestRisk = 0.2f;
		S.JobRollSeeds.Add(J.Id, 12345);
		return S.Jobs.Add_GetRef(J);
	}
	static void ResolveWith(UDCRunSubsystem& S, const FDCCrewPlan& P, const FDCSuccessBreakdown& Bd, const FDCJob& J, FRandomStream& R, FDCHeistResult& Out)
	{
		FDCSimImpl::ResolveHeist(S, P, Bd, J, R, Out);
	}
};

namespace DCSimTest
{
	inline TStrongObjectPtr<UDCRunSubsystem> NewSim(bool bFixtures = false)
	{
		// Subsystems must live inside a GameInstance (ClassWithin). The GI is kept alive as the subsystem's outer.
		UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());
		TStrongObjectPtr<UDCRunSubsystem> S(NewObject<UDCRunSubsystem>(GI));
		if (bFixtures) { FDCSimTestAccess::SetDataDir(*S, FDCSimData::FixturesDir()); }
		return S;
	}

	/** Best available crew for a job: required roles first (strongest by weighted stats), then helpful extras. */
	inline FDCCrewPlan BuildCrew(const UDCRunSubsystem& S, const FDCJob& J, float Cut, TSet<int32>& Used)
	{
		FDCCrewPlan P;
		P.JobId = J.Id;
		P.Cut = Cut;
		auto Score = [&S, &J](const FDCCriminal& C)
		{
			const FDCStatBlock E = S.GetEffectiveStats(C.Id);
			float V = 0.f;
			for (const TPair<EDCStat, float>& W : J.StatWeights) { V += W.Value * E.Get(W.Key); }
			return V;
		};
		for (EDCCriminalType Role : J.RequiredRoles)
		{
			const FDCCriminal* Best = nullptr;
			for (const FDCCriminal& C : S.GetRoster())
			{
				if (C.Type != Role || !C.IsAvailable() || Used.Contains(C.Id) || P.CriminalIds.Contains(C.Id)) { continue; }
				if (!Best || Score(C) > Score(*Best)) { Best = &C; }
			}
			if (Best) { P.CriminalIds.Add(Best->Id); }
		}
		for (const FDCCriminal& C : S.GetRoster())
		{
			if (P.CriminalIds.Num() >= J.MaxCrew) { break; }
			if (!C.IsAvailable() || Used.Contains(C.Id) || P.CriminalIds.Contains(C.Id)) { continue; }
			if (J.BonusRoles.Contains(C.Type)) { P.CriminalIds.Add(C.Id); }
		}
		return P;
	}

	/** A sane bot: builds rooms, hires, buys gear, picks perks, commits the best plans. Returns committed count. */
	inline int32 BotPlan(UDCRunSubsystem& S, bool bSpend = true)
	{
		if (bSpend)
		{
			if (S.HasPendingPerkOffer()) { S.PickPerk(S.GetPerkOffer()[0].Id); }
			for (const FDCRoom& R : S.GetRooms())
			{
				if (R.bCanAct && S.GetCash() > R.NextCost + 12000) { if (R.State == EDCRoomState::Built) { S.UpgradeRoom(R.Type); } else { S.BuildRoom(R.Type); } }
			}
			if (S.GetRoster().Num() < S.GetRosterCap())
			{
				for (const FDCCriminal& R : S.GetRecruits())
				{
					if (S.GetCash() > R.HireCost + 8000) { S.Hire(R.Id); break; }
				}
			}
			const TArray<FDCGearItem> Cat = S.GetGearCatalogue();
			for (const FDCCriminal& C : TArray<FDCCriminal>(S.GetRoster()))
			{
				for (const FDCGearItem& G : Cat)
				{
					if (G.bAvailable && !C.Equipped.Contains(G.Slot) && S.GetCash() > G.Price + 10000) { S.BuyGear(G.Id, C.Id); break; }
				}
			}
			for (const FDCCriminal& C : TArray<FDCCriminal>(S.GetRoster()))
			{
				const int32 Cost = S.GetPatchUpCost(C.Id);
				if (Cost > 0 && S.GetCash() > Cost + 10000) { S.PatchUp(C.Id); }
			}
			for (const FDCCriminal& C : TArray<FDCCriminal>(S.GetFallen()))
			{
				const int32 Cost = S.GetBailCost(C.Id);
				if (Cost > 0 && S.GetCash() > Cost + 10000) { S.BailOut(C.Id); }
			}
		}
		struct FCand { FDCCrewPlan Plan; float Value; };
		int32 Committed = 0;
		TSet<int32> Used;
		for (int32 Pass = 0; Pass < S.GetMaxCrewsPerNight(); ++Pass)
		{
			TArray<FCand> Cands;
			for (const FDCJob& J : S.GetJobs())
			{
				if (J.bCommitted) { continue; }
				for (float Cut : { 0.35f, 0.30f, 0.25f, 0.20f })
				{
					const FDCCrewPlan P = BuildCrew(S, J, Cut, Used);
					const FDCSuccessBreakdown Bd = S.PreviewPlan(P);
					if (Bd.bValid) { Cands.Add({ P, Bd.SuccessChance * Bd.DirectorTake }); break; }
				}
			}
			if (Cands.Num() == 0) { break; }
			Cands.Sort([](const FCand& A, const FCand& B) { return A.Value > B.Value; });
			if (S.CommitPlan(Cands[0].Plan)) { ++Committed; for (int32 Id : Cands[0].Plan.CriminalIds) { Used.Add(Id); } }
			else { break; }
		}
		return Committed;
	}

	inline FString DescribeState(const UDCRunSubsystem& S)
	{
		FString Out = FString::Printf(TEXT("N%d C%d H%d T%d|"), S.GetNight(), S.GetCash(), S.GetHeistsCompleted(), S.GetUnlockedTier());
		for (const FDCCriminal& C : S.GetRoster())
		{
			Out += FString::Printf(TEXT("%d:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d;"), C.Id, *C.GetDisplayName(), (int32)C.Type, C.Level, C.XP, C.Loyalty, C.Luck, (int32)C.Status,
				C.BaseStats.Driving, C.BaseStats.Muscle, C.BaseStats.Tech, C.BaseStats.Finesse);
		}
		Out += TEXT("|");
		for (const FDCJob& J : S.GetJobs()) { Out += FString::Printf(TEXT("%s:%d:%d:%.3f;"), *J.TemplateId.ToString(), J.Tier, J.Payout, J.Difficulty); }
		Out += TEXT("|");
		for (const FDCCriminal& C : S.GetRecruits()) { Out += FString::Printf(TEXT("%s:%d;"), *C.GetDisplayName(), C.HireCost); }
		Out += TEXT("|");
		for (const FDCHeistResult& R : S.GetLastNightReport().Results) { Out += FString::Printf(TEXT("%s:%.5f:%d:%d;"), *R.JobName, R.Roll, (int32)R.Outcome, R.DirectorTake); }
		return Out;
	}
}

#endif
