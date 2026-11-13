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
	static void SetSlot(UDCRunSubsystem& S, const FString& Slot) { S.SaveSlotOverride = Slot; }
	static void EnableSave(UDCRunSubsystem& S, bool b) { S.bSaveEnabled = b; }
	static FDCSimData& Data(UDCRunSubsystem& S) { return *S.Data; }
	static int32& Cash(UDCRunSubsystem& S) { return S.Cash; }
	static int32& Successes(UDCRunSubsystem& S) { return S.MissionsSucceeded; }
	static double& RunTime(UDCRunSubsystem& S) { return S.RunTime; }
	static TArray<FDCCriminal>& Roster(UDCRunSubsystem& S) { return S.Roster; }
	static TArray<FDCMission>& Missions(UDCRunSubsystem& S) { return S.Missions; }
	static TArray<FDCPerk>& PerksTaken(UDCRunSubsystem& S) { return S.PerksTaken; }
	static TArray<FDCRoom>& Rooms(UDCRunSubsystem& S) { return S.Rooms; }
	static double& NextPostAt(UDCRunSubsystem& S) { return S.NextPostAt; }
	static int32 NewId(UDCRunSubsystem& S) { return S.NextId++; }

	static void SetRoom(UDCRunSubsystem& S, EDCRoomType T, int32 Level)
	{
		for (FDCRoom& R : S.Rooms) { if (R.Type == T) { R.Level = Level; R.State = Level > 0 ? EDCRoomState::Built : EDCRoomState::Locked; } }
	}
	static void AddPerk(UDCRunSubsystem& S, FName Id)
	{
		if (const FDCPerkDef* P = S.Data->Perks.Find(Id)) { S.PerksTaken.Add(P->Perk); }
	}
	static FDCCriminal& AddCriminal(UDCRunSubsystem& S, EDCCriminalType Type, int32 Level, TArray<FName> Abilities, TArray<FName> Traits = {}, int32 Loyalty = 50)
	{
		FDCCriminal C;
		C.Id = S.NextId++;
		C.FirstName = FString::Printf(TEXT("Test%d"), C.Id);
		C.Nickname = TEXT("Nick");
		C.LastName = TEXT("Tester");
		C.Type = Type;
		C.Level = Level;
		C.XPToNext = FDCSimImpl::XPToNext(S, Level);
		C.Abilities = Abilities;
		C.Traits = Traits;
		C.Loyalty = Loyalty;
		return S.Roster.Add_GetRef(C);
	}
	static FDCMission& AddMission(UDCRunSubsystem& S, int32 Level, int32 CrewSize, TArray<EDCThreat> Threats, float Duration = 200.f, int32 Cost = 0, int32 Payout = 10000)
	{
		FDCMission M;
		M.Id = S.NextId++;
		M.TemplateId = TEXT("test_mission");
		M.DisplayName = TEXT("Test Mission");
		M.Tier = 1;
		M.Level = Level;
		M.CrewSize = CrewSize;
		M.Threats = Threats;
		M.Duration = Duration;
		M.Cost = Cost;
		M.Payout = Payout;
		M.XPReward = 50;
		M.ArrestRisk = 0.f;
		M.ExpiresAt = -1.0;
		return S.Missions.Add_GetRef(M);
	}
	/** Puts a mission straight into progress with a forced chance (2 = certain success, 0 = certain failure). */
	static void ForceRun(UDCRunSubsystem& S, FDCMission& M, TArray<int32> Crew, float Chance, float Cut = 0.3f)
	{
		M.bInProgress = true; M.StartedAt = S.RunTime; M.EndsAt = S.RunTime + M.Duration; M.CrewIds = Crew; M.Cut = Cut; M.LockedChance = Chance;
		for (int32 Id : Crew) { if (FDCCriminal* C = FDCSimImpl::RosterMut(S, Id)) { C->Status = EDCCriminalStatus::OnMission; C->ActiveMissionId = M.Id; } }
	}
	/** Empty run (fixtures, no saves): Office only, no crew, no board, no recruits. */
	static void ClearRun(UDCRunSubsystem& S)
	{
		S.Roster.Reset(); S.Missions.Reset(); S.Recruits.Reset(); S.Reports.Reset(); S.Stash.Reset(); S.PerkOffer.Reset();
		S.NextPostAt = 1e12; S.RecruitRefreshAt = 1e12;
	}
};

namespace DCSimTest
{
	inline TStrongObjectPtr<UDCRunSubsystem> NewSim(const TCHAR* Slot = TEXT("DCRun_Test"), bool bSave = false)
	{
		// Subsystems must live inside a GameInstance (ClassWithin).
		UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());
		TStrongObjectPtr<UDCRunSubsystem> S(NewObject<UDCRunSubsystem>(GI));
		FDCSimTestAccess::SetDataDir(*S, FDCSimData::FixturesDir());
		FDCSimTestAccess::SetSlot(*S, Slot);
		FDCSimTestAccess::EnableSave(*S, bSave);
		return S;
	}

	/** Stops a test sim from ticking/saving after the test (it lingers until GC). */
	struct FEndGuard
	{
		UDCRunSubsystem* S;
		explicit FEndGuard(UDCRunSubsystem* In) : S(In) {}
		~FEndGuard() { if (S) { FDCSimTestAccess::EnableSave(*S, false); S->ExitToTitle(); } }
	};

	inline FString DescribeState(const UDCRunSubsystem& S)
	{
		FString Out = FString::Printf(TEXT("T%.3f C%d S%d D%d Tier%d|"), S.GetRunTime(), S.GetCash(), S.GetMissionsSucceeded(), S.GetDay(), S.GetUnlockedTier());
		for (const FDCCriminal& C : S.GetRoster())
		{
			Out += FString::Printf(TEXT("%d:%s:%d:%d:L%d:%d:%d:%d:%d:%.2f;"), C.Id, *C.GetDisplayName(), (int32)C.Type, (int32)C.Quality, C.Level, C.XP, C.Loyalty, (int32)C.Status, C.ActiveMissionId, C.InjuredUntil);
			for (FName A : C.Abilities) { Out += A.ToString() + TEXT(","); }
			for (FName T : C.Traits) { Out += T.ToString() + TEXT(","); }
		}
		Out += TEXT("|");
		for (const FDCMission& M : S.GetMissions())
		{
			Out += FString::Printf(TEXT("%d:%s:%d:%d:%.1f:%d:%d:%.2f:%d:%.3f:%.4f;"), M.Id, *M.TemplateId.ToString(), M.Level, M.CrewSize, M.Duration, M.Cost, M.Payout, M.ExpiresAt, M.bInProgress ? 1 : 0, M.EndsAt, M.LockedChance);
		}
		Out += TEXT("|");
		for (const FDCCriminal& C : S.GetRecruits()) { Out += FString::Printf(TEXT("%s:%d;"), *C.GetDisplayName(), C.HireCost); }
		Out += TEXT("|");
		for (const FDCMissionReport& R : S.GetReports()) { Out += FString::Printf(TEXT("%d:%.5f:%d:%d:%.2f;"), R.ReportId, R.Roll, (int32)R.Outcome, R.DirectorTake, R.CompletedAt); }
		Out += TEXT("|");
		for (const FDCRoom& R : S.GetRooms()) { Out += FString::Printf(TEXT("%d:%d:%.1f;"), (int32)R.State, R.Level, R.BuildEndsAt); }
		for (FName G : S.GetStash()) { Out += G.ToString() + TEXT(","); }
		for (const FDCPerk& P : S.GetPerksTaken()) { Out += P.Id.ToString() + TEXT(","); }
		Out += FString::Printf(TEXT("|next %.2f rec %.2f"), S.GetNextMissionIn(), S.GetRecruitRefreshIn());
		return Out;
	}

	/** One bot decision pass (no time advance). Returns the number of missions started. */
	inline int32 BotStep(UDCRunSubsystem& S)
	{
		if (S.HasPendingPerkOffer()) { const TArray<FDCPerk> O = S.GetPerkOffer(); if (O.Num()) { S.PickPerk(O[0].Id); } }
		if (S.GetRoster().Num() < 4)
		{
			const FDCCriminal* Best = nullptr;
			for (const FDCCriminal& R : S.GetRecruits()) { if (R.HireCost <= S.GetCash() && (!Best || R.HireCost < Best->HireCost)) { Best = &R; } }
			if (Best) { S.Hire(Best->Id); }
		}
		const FDCRoom* Cheapest = nullptr;
		const TArray<FDCRoom> Rooms = S.GetRooms();
		for (const FDCRoom& R : Rooms) { if (R.bCanAct && (!Cheapest || R.NextCost < Cheapest->NextCost)) { Cheapest = &R; } }
		if (Cheapest && S.GetCash() > 2 * Cheapest->NextCost) { S.BuildRoom(Cheapest->Type); }
		int32 Started = 0;
		TArray<int32> Ids;
		for (const FDCMission& M : S.GetMissions()) { if (!M.bInProgress) { Ids.Add(M.Id); } }
		for (int32 Id : Ids)
		{
			const FDCCrewPlan P = S.SuggestPlan(Id, 0.3f);
			const FDCSuccessBreakdown B = S.PreviewPlan(P);
			if (B.bValid && B.SuccessChance >= 0.5f && S.StartMission(P)) { ++Started; }
		}
		return Started;
	}
}

#endif
