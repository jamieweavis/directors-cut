// Director's Cut — DirectorsCut.Sim.* automation tests (Agent A).
#include "Misc/AutomationTest.h"
#include "Tests/Sim/DCSimTestUtils.h"
#include "DirectorsCut.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace DCSimTest;
using TA = FDCSimTestAccess;

static constexpr EAutomationTestFlags DCSimFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

/** Starts a run and clears roster + board so tests can hand-build a scenario. */
static void CleanSlate(UDCRunSubsystem& S, int32 Seed = 99)
{
	S.StartNewRun(Seed);
	TA::Roster(S).Reset();
	TA::Jobs(S).Reset();
	TA::Cash(S) = 1000000;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimDataLoadTest, "DirectorsCut.Sim.DataLoad", DCSimFlags)
bool FDCSimDataLoadTest::RunTest(const FString&)
{
	for (bool bFixtures : { false, true })
	{
		TStrongObjectPtr<UDCRunSubsystem> S = NewSim(bFixtures);
		S->StartNewRun(1);
		const FDCSimData& D = TA::Data(*S);
		const FString Ctx = bFixtures ? TEXT("fixtures") : TEXT("content");
		TestEqual(Ctx + TEXT(": 6 types"), D.Types.Num(), 6);
		TestEqual(Ctx + TEXT(": 8 rooms"), D.Rooms.Num(), 8);
		TestTrue(Ctx + TEXT(": >= 8 jobs"), D.Jobs.Num() >= 8);
		TestTrue(Ctx + TEXT(": >= 4 tier-1 gear"), D.Gear.FilterByPredicate([](const FDCGearDef& G) { return G.Item.Tier == 1; }).Num() >= 4);
		TestTrue(Ctx + TEXT(": >= 15 perks"), D.Perks.Num() >= 15);
		TestEqual(Ctx + TEXT(": exactly one Final Cut"), D.Jobs.FilterByPredicate([](const FDCJobDef& J) { return J.bFinalCut; }).Num(), 1);
		TestTrue(Ctx + TEXT(": names"), D.Names.First.Num() >= 4 && D.Names.Last.Num() >= 4 && D.Names.Nicknames.Num() >= 4);
		for (const FString& W : D.LoadWarnings) { AddWarning(Ctx + TEXT(": ") + W); }
		for (const FDCJobDef& J : D.Jobs)
		{
			float Sum = 0.f;
			for (const auto& P : J.StatWeights) { Sum += P.Value; }
			TestTrue(FString::Printf(TEXT("%s weights sum to 1"), *J.Id.ToString()), FMath::IsNearlyEqual(Sum, 1.f, 0.011f));
		}
	}
	// Missing directory: everything falls back to fixtures, then built-in defaults; never crashes.
	TSharedRef<FDCSimData> Missing = FDCSimData::Load(TEXT("/nonexistent/dir"));
	TestEqual(TEXT("fallback types"), Missing->Types.Num(), 6);
	TestTrue(TEXT("fallback jobs (fixtures)"), Missing->Jobs.Num() >= 8);
	FDCBalance Defaults;
	TestEqual(TEXT("default startingCash"), Defaults.StartingCash, 25000);
	TestEqual(TEXT("default refusal"), Defaults.RefusalThreshold, 0.35f);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimFormulaTest, "DirectorsCut.Sim.Formula", DCSimFlags)
bool FDCSimFormulaTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim(true);
	UDCRunSubsystem& S = *SP;
	CleanSlate(S);
	const FDCBalance& B = TA::Data(S).Balance;

	FDCCriminal& M1 = TA::AddCriminal(S, EDCCriminalType::Muscle, 50, 50, 5);
	const int32 M1Id = M1.Id;
	FDCJob& J = TA::AddJob(S, { EDCCriminalType::Muscle }, 50.f, 1, 3);
	J.BonusRoles = { EDCCriminalType::Wheelman };
	const int32 JId = J.Id;

	FDCCrewPlan P; P.JobId = JId; P.CriminalIds = { M1Id }; P.Cut = B.FairCutBase;

	// Margin 0, fair cut, no bonuses -> SkillBase.
	FDCSuccessBreakdown Bd = S.PreviewPlan(P);
	TestTrue(TEXT("valid"), Bd.bValid);
	TestEqual(TEXT("crew score"), Bd.CrewScore, 50.f);
	TestEqual(TEXT("margin 0"), Bd.Margin, 0.f);
	TestEqual(TEXT("skill = base"), Bd.SkillChance, B.SkillBase, 1e-4f);
	TestEqual(TEXT("willingness 1"), Bd.Willingness.FindRef(M1Id), 1.f, 1e-4f);
	TestEqual(TEXT("no generosity at exactly fair"), Bd.GenerosityBonus, 0.f);
	TestEqual(TEXT("success = skill"), Bd.SuccessChance, B.SkillBase, 1e-4f);
	TestEqual(TEXT("clean threshold"), Bd.CleanThreshold, B.SkillBase * B.CleanBand, 1e-4f);
	TestEqual(TEXT("lucky break 2% x luck"), Bd.LuckyBreakChance, B.LuckyBreakPerLuck * 5, 1e-4f);
	TestTrue(TEXT("has lines"), Bd.Lines.Num() >= 6);

	// Generosity.
	P.Cut = B.FairCutBase - 0.05f;
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("generosity bonus"), Bd.GenerosityBonus, B.GenerosityBonus, 1e-4f);
	TestEqual(TEXT("success with generosity"), Bd.SuccessChance, B.SkillBase + B.GenerosityBonus, 1e-4f);

	// Willingness + refusal threshold across the whole cut range.
	for (float Cut = B.CutMin; Cut <= B.CutMax + 0.001f; Cut += B.CutStep)
	{
		P.Cut = Cut;
		Bd = S.PreviewPlan(P);
		const float Fair = B.FairCutBase;
		const float Expect = FMath::Clamp(1.f - B.WillSlope * FMath::Max(0.f, Cut - Fair), B.WillMin, 1.f);
		TestEqual(FString::Printf(TEXT("will @%.2f"), Cut), Bd.Willingness.FindRef(M1Id), Expect, 1e-4f);
		TestEqual(FString::Printf(TEXT("refusal @%.2f"), Cut), Bd.RefusingCriminalIds.Contains(M1Id), Expect < B.RefusalThreshold);
		TestEqual(FString::Printf(TEXT("valid @%.2f"), Cut), Bd.bValid, Expect >= B.RefusalThreshold);
		TestEqual(FString::Printf(TEXT("willfactor @%.2f"), Cut), Bd.WillFactor, B.WillFactorBase + B.WillFactorScale * Expect, 1e-4f);
		const float Raw = Bd.SkillChance + Bd.RoleFitBonus + Bd.GenerosityBonus + Bd.ModifierBonus;
		TestEqual(FString::Printf(TEXT("success formula @%.2f"), Cut), Bd.SuccessChance, FMath::Clamp(Raw * Bd.WillFactor, B.SuccessMin, B.SuccessMax), 1e-4f);
		if (!Bd.bValid) { TestFalse(TEXT("refusal has reason"), Bd.InvalidReason.IsEmpty()); }
	}

	// Loyalty shifts fair cut.
	TA::Roster(S)[0].Loyalty = 100;
	P.Cut = B.FairCutBase;
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("fair cut loyalty 100"), Bd.FairCut.FindRef(M1Id), B.FairCutBase + 50.f / B.FairCutLoyaltyDiv, 1e-4f);
	TA::Roster(S)[0].Loyalty = 50;

	// Clamps.
	TA::Jobs(S)[0].Difficulty = 1.f;
	P.Cut = B.FairCutBase;
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("skill max clamp"), Bd.SkillChance, B.SkillMax, 1e-4f);
	P.Cut = B.CutMin;
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("success max clamp"), Bd.SuccessChance, FMath::Min(B.SuccessMax, B.SkillMax + B.GenerosityBonus), 1e-4f);
	TestEqual(TEXT("no arrest with huge margin"), Bd.ArrestChanceOnFail, B.ArrestRiskPer5Heists * (S.GetHeistsCompleted() / 5), 1e-4f);
	TA::Jobs(S)[0].Difficulty = 100000.f;
	P.Cut = B.FairCutBase;
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("skill min clamp"), Bd.SkillChance, B.SkillMin, 1e-4f);
	TestTrue(TEXT("success >= min"), Bd.SuccessChance >= B.SuccessMin - 1e-4f);
	TestTrue(TEXT("arrest capped"), Bd.ArrestChanceOnFail <= 0.95f + 1e-4f);
	TA::Jobs(S)[0].Difficulty = 50.f;

	// Support weight + role fit.
	FDCCriminal& M2 = TA::AddCriminal(S, EDCCriminalType::Muscle, 40);
	FDCCriminal& W1 = TA::AddCriminal(S, EDCCriminalType::Wheelman, 20);
	const int32 M2Id = M2.Id, W1Id = W1.Id;
	P.CriminalIds = { M1Id, M2Id };
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("support weight"), Bd.CrewScore, 50.f + B.SupportWeight * 40.f, 1e-3f);
	TestEqual(TEXT("no role fit for 2nd muscle"), Bd.RoleFitBonus, 0.f);
	P.CriminalIds = { M1Id, W1Id };
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("role fit for bonus Wheelman"), Bd.RoleFitBonus, B.RoleFitPer, 1e-4f);

	// Invalid plans.
	P.CriminalIds = { W1Id };
	Bd = S.PreviewPlan(P);
	TestFalse(TEXT("missing role invalid"), Bd.bValid);
	TestTrue(TEXT("missing Muscle listed"), Bd.MissingRoles.Contains(EDCCriminalType::Muscle));
	P.CriminalIds = { M1Id, M2Id, W1Id };
	TA::Jobs(S)[0].MaxCrew = 2;
	Bd = S.PreviewPlan(P);
	TestFalse(TEXT("crew too large"), Bd.bValid);
	TA::Jobs(S)[0].MaxCrew = 3;
	P.CriminalIds = { M1Id, M1Id };
	TestFalse(TEXT("duplicate invalid"), S.PreviewPlan(P).bValid);
	P.CriminalIds = { M1Id };
	TA::Roster(S)[0].Status = EDCCriminalStatus::Injured; TA::Roster(S)[0].InjuredNights = 2;
	Bd = S.PreviewPlan(P);
	TestFalse(TEXT("injured invalid"), Bd.bValid);
	TestTrue(TEXT("injured reason"), Bd.InvalidReason.Contains(TEXT("injured")));
	TA::Roster(S)[0].Status = EDCCriminalStatus::Available; TA::Roster(S)[0].InjuredNights = 0;
	P.JobId = 999999;
	TestFalse(TEXT("unknown job invalid"), S.PreviewPlan(P).bValid);
	P.JobId = JId;

	// Capacity: one crew per night without a Garage.
	FDCJob& J2 = TA::AddJob(S, { EDCCriminalType::Wheelman }, 30.f);
	const int32 J2Id = J2.Id;
	TestTrue(TEXT("commit first"), S.CommitPlan(P));
	TestEqual(TEXT("member on job"), S.FindCriminal(M1Id)->Status, EDCCriminalStatus::OnJob);
	TestTrue(TEXT("committed plan still previews valid"), S.PreviewPlan(P).bValid);
	FDCCrewPlan P2; P2.JobId = J2Id; P2.CriminalIds = { W1Id }; P2.Cut = 0.3f;
	Bd = S.PreviewPlan(P2);
	TestFalse(TEXT("capacity invalid"), Bd.bValid);
	FDCCrewPlan P3 = P2; P3.JobId = J2Id; P3.CriminalIds = { M1Id, W1Id };
	TestFalse(TEXT("already cast"), S.PreviewPlan(P3).bValid);
	S.CancelPlan(JId);
	TestEqual(TEXT("cancel frees"), S.FindCriminal(M1Id)->Status, EDCCriminalStatus::Available);
	TestTrue(TEXT("now second job valid"), S.PreviewPlan(P2).bValid);

	// Perks on willingness.
	TA::AddPerk(S, TEXT("auteur"));
	P.Cut = B.CutMax;
	Bd = S.PreviewPlan(P);
	TestTrue(TEXT("auteur floor"), Bd.Willingness.FindRef(M1Id) >= 0.6f - 1e-4f);
	TestTrue(TEXT("auteur: no refusal"), Bd.bValid);
	TA::PerksTaken(S).Reset();
	TA::AddPerk(S, TEXT("union_rates"));
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("union rates fair cut"), Bd.FairCut.FindRef(M1Id), B.FairCutBase + 0.05f, 1e-4f);
	TA::PerksTaken(S).Reset();

	// Outcome bands.
	TestEqual(TEXT("band clean"), FDCSimImpl::OutcomeForRoll(0.10f, 0.5f, 0.3f, 0.1f), EDCHeistOutcome::Clean);
	TestEqual(TEXT("band messy"), FDCSimImpl::OutcomeForRoll(0.40f, 0.5f, 0.3f, 0.1f), EDCHeistOutcome::Messy);
	TestEqual(TEXT("band failed"), FDCSimImpl::OutcomeForRoll(0.60f, 0.5f, 0.3f, 0.1f), EDCHeistOutcome::Failed);
	TestEqual(TEXT("band busted"), FDCSimImpl::OutcomeForRoll(0.95f, 0.5f, 0.3f, 0.1f), EDCHeistOutcome::Busted);
	TestEqual(TEXT("success beats busted band"), FDCSimImpl::OutcomeForRoll(0.95f, 0.97f, 0.5f, 0.1f), EDCHeistOutcome::Messy);

	// Purity: PreviewPlan never mutates.
	const FString Before = DescribeState(S);
	for (int32 i = 0; i < 5; ++i) { S.PreviewPlan(P); S.PreviewPlan(P2); }
	TestEqual(TEXT("preview is pure"), DescribeState(S), Before);

	// Lucky break: chance 1.0 always re-rolls a failed first roll; chance 0 never does.
	{
		FDCSuccessBreakdown LB = S.PreviewPlan(P2);
		LB.SuccessChance = 0.3f; LB.CleanThreshold = 0.18f; LB.ArrestChanceOnFail = 0.f;
		int32 SeedFail = 1;
		while (FRandomStream(SeedFail).FRand() < 0.3f) { ++SeedFail; }
		for (float Chance : { 1.f, 0.f })
		{
			LB.LuckyBreakChance = Chance;
			FRandomStream Expect(SeedFail);
			const float R1 = Expect.FRand(); Expect.FRand(); const float R3 = Expect.FRand();
			FRandomStream Dice(SeedFail);
			FDCHeistResult Out;
			TA::ResolveWith(S, P2, LB, *S.FindJob(J2Id), Dice, Out);
			TestEqual(TEXT("first roll recorded"), Out.FirstRoll, R1);
			TestEqual(TEXT("lucky break fired"), Out.bLuckyBreak, Chance > 0.5f);
			TestEqual(TEXT("final roll"), Out.Roll, Chance > 0.5f ? R3 : R1);
			TestFalse(TEXT("flavour line"), Out.FlavourLine.IsEmpty());
			TestFalse(TEXT("flavour substituted"), Out.FlavourLine.Contains(TEXT("{name}")));
		}
	}

	// Money formatting.
	TestEqual(TEXT("money"), FDCSimImpl::Money(12500), FString(TEXT("$12,500")));
	TestEqual(TEXT("money small"), FDCSimImpl::Money(900), FString(TEXT("$900")));
	TestEqual(TEXT("money big"), FDCSimImpl::Money(1234567), FString(TEXT("$1,234,567")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimPreviewMatchesNightTest, "DirectorsCut.Sim.PreviewMatchesNight", DCSimFlags)
bool FDCSimPreviewMatchesNightTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim();
	UDCRunSubsystem& S = *SP;
	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		S.StartNewRun(Seed);
		BotPlan(S, false);
		TArray<float> Chances;
		for (const FDCCrewPlan& P : S.GetCommittedPlans()) { Chances.Add(S.PreviewPlan(P).SuccessChance); }
		const TArray<FDCHeistResult> Res = S.EndNight();
		TestEqual(TEXT("one result per plan"), Res.Num(), Chances.Num());
		for (int32 i = 0; i < FMath::Min(Res.Num(), Chances.Num()); ++i) { TestEqual(TEXT("preview == resolved chance"), Res[i].SuccessChance, Chances[i]); }
		TestEqual(TEXT("phase dailies"), S.GetPhase(), EDCGamePhase::Dailies);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimDeterminismTest, "DirectorsCut.Sim.Determinism", DCSimFlags)
bool FDCSimDeterminismTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> A = NewSim();
	TStrongObjectPtr<UDCRunSubsystem> B = NewSim();
	for (int32 Seed : { 4242, 7, 123456 })
	{
		A->StartNewRun(Seed);
		B->StartNewRun(Seed);
		TestEqual(TEXT("seed stored"), A->GetSeed(), Seed);
		TestEqual(TEXT("initial state"), DescribeState(*A), DescribeState(*B));
		for (int32 N = 0; N < 20 && A->GetPhase() == EDCGamePhase::Planning; ++N)
		{
			BotPlan(*A); BotPlan(*B);
			TestEqual(FString::Printf(TEXT("seed %d night %d plan"), Seed, N), DescribeState(*A), DescribeState(*B));
			A->EndNight(); B->EndNight();
			TestEqual(FString::Printf(TEXT("seed %d night %d result"), Seed, N), DescribeState(*A), DescribeState(*B));
			if (A->GetRetakesRemaining() > 0)
			{
				for (const FDCHeistResult& R : TArray<FDCHeistResult>(A->GetLastNightReport().Results)) { if (R.bRetakeAvailable) { A->UseRetake(R.ResultId); B->UseRetake(R.ResultId); break; } }
			}
			A->FinishDailies(); B->FinishDailies();
			TestEqual(FString::Printf(TEXT("seed %d night %d after"), Seed, N), DescribeState(*A), DescribeState(*B));
		}
	}
	A->StartNewRun(1); B->StartNewRun(2);
	TestNotEqual(TEXT("different seeds differ"), DescribeState(*A), DescribeState(*B));
	A->StartNewRun(0);
	TestNotEqual(TEXT("seed 0 picks a seed"), A->GetSeed(), 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimGenerationTest, "DirectorsCut.Sim.Generation", DCSimFlags)
bool FDCSimGenerationTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim();
	UDCRunSubsystem& S = *SP;
	int32 Failures = 0;
	for (int32 Seed = 1; Seed <= 200; ++Seed)
	{
		S.StartNewRun(Seed);
		const FDCBalance& B = TA::Data(S).Balance;
		const TArray<FDCCriminal>& R = S.GetRoster();
		TSet<EDCCriminalType> Types;
		TArray<EDCCriminalType> Pool;
		for (const FDCCriminal& C : R)
		{
			Types.Add(C.Type); Pool.Add(C.Type);
			const FString N = C.GetDisplayName();
			if (C.FirstName.IsEmpty() || C.Nickname.IsEmpty() || C.LastName.IsEmpty() || !N.Contains(TEXT("\""))) { ++Failures; AddError(FString::Printf(TEXT("seed %d bad name %s"), Seed, *N)); }
			for (int32 i = 0; i < (int32)EDCStat::MAX; ++i) { const int32 V = C.BaseStats.Get((EDCStat)i); if (V < 1 || V > 100) { ++Failures; AddError(TEXT("stat out of range")); } }
			if (C.Luck < 0 || C.Luck > 10 || C.Loyalty < 0 || C.Loyalty > 100) { ++Failures; AddError(TEXT("luck/loyalty out of range")); }
		}
		if (R.Num() != B.StartingRoster) { ++Failures; AddError(FString::Printf(TEXT("seed %d roster %d"), Seed, R.Num())); }
		if (Types.Num() < 3) { ++Failures; AddError(FString::Printf(TEXT("seed %d only %d types"), Seed, Types.Num())); }
		if (S.GetJobs().Num() != B.StartingBoard) { ++Failures; AddError(FString::Printf(TEXT("seed %d board %d"), Seed, S.GetJobs().Num())); }
		int32 Doable = 0;
		for (const FDCJob& J : S.GetJobs())
		{
			if (J.Tier != 1) { ++Failures; AddError(TEXT("night-1 job above tier 1")); }
			if (J.bHeavy || J.bFinalCut) { ++Failures; AddError(TEXT("heavy/final job on night 1")); }
			if (J.Payout <= 0 || J.Difficulty <= 0.f) { ++Failures; AddError(TEXT("bad job numbers")); }
			const FDCJobDef* Def = TA::Data(S).FindJob(J.TemplateId);
			if (Def && FDCSimImpl::IsDoable(*Def, Pool)) { ++Doable; }
		}
		if (Doable < 2) { ++Failures; AddError(FString::Printf(TEXT("seed %d only %d doable jobs"), Seed, Doable)); }
		if (S.GetRecruits().Num() != B.RecruitBoardSize) { ++Failures; AddError(TEXT("recruit board size")); }
		for (const FDCCriminal& C : S.GetRecruits()) { if (C.HireCost <= 0) { ++Failures; AddError(TEXT("recruit cost")); } }
		TestEqual(TEXT("cash"), S.GetCash(), B.StartingCash);
		TestEqual(TEXT("office built L1"), S.GetRoom(EDCRoomType::Office).Level, 1);
		if (Failures > 20) { break; }
	}
	TestEqual(TEXT("generation failures"), Failures, 0);

	// Recruits refresh every recruitRefreshNights; later boards keep >= 2 doable jobs.
	S.StartNewRun(55);
	const int32 Refresh = TA::Data(S).Balance.RecruitRefreshNights;
	const FString FirstRecruits = S.GetRecruits().Num() ? S.GetRecruits()[0].GetDisplayName() : FString();
	for (int32 N = 0; N < Refresh; ++N)
	{
		TA::Cash(S) = 100000;
		S.EndNight();
		S.FinishDailies();
		TArray<EDCCriminalType> Pool;
		for (const FDCCriminal& C : S.GetRoster()) { Pool.Add(C.Type); }
		int32 Doable = 0;
		for (const FDCJob& J : S.GetJobs()) { if (const FDCJobDef* Def = TA::Data(S).FindJob(J.TemplateId)) { Doable += FDCSimImpl::IsDoable(*Def, Pool) ? 1 : 0; } }
		TestTrue(TEXT("later board doable"), Doable >= 2);
		TestEqual(TEXT("board size after night 1"), S.GetJobs().Num(), TA::Data(S).Balance.BaseJobBoard);
	}
	TestTrue(TEXT("recruits refreshed event"), S.GetLastNightReport().Events.Contains(TA::Data(S).Strings.RecruitsRefreshed));
	TestEqual(TEXT("refresh counter reset"), S.GetRecruitRefreshIn(), Refresh);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimModifierTest, "DirectorsCut.Sim.Modifiers", DCSimFlags)
bool FDCSimModifierTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim(true);
	UDCRunSubsystem& S = *SP;
	S.StartNewRun(5);
	const FDCBalance& B = TA::Data(S).Balance;
	TA::Cash(S) = 1000000;

	// Roster cap: Lounge.
	TestEqual(TEXT("base roster cap"), S.GetRosterCap(), B.BaseRosterCap);
	TestTrue(TEXT("build lounge"), S.BuildRoom(EDCRoomType::Lounge));
	TestEqual(TEXT("lounge L1 cap"), S.GetRosterCap(), 6);
	TestTrue(TEXT("upgrade lounge"), S.UpgradeRoom(EDCRoomType::Lounge));
	TestEqual(TEXT("lounge L2 cap"), S.GetRosterCap(), 8);
	TestEqual(TEXT("lounge level"), S.GetRoom(EDCRoomType::Lounge).Level, 2);
	TestFalse(TEXT("cannot build twice"), S.BuildRoom(EDCRoomType::Lounge));

	// Upkeep includes rooms.
	const int32 RoomUpkeep = TA::Data(S).GetRoom(EDCRoomType::Lounge).UpkeepPerNight + TA::Data(S).GetRoom(EDCRoomType::Office).UpkeepPerNight;
	TestEqual(TEXT("upkeep"), S.GetUpkeepDue(), B.UpkeepPerCriminal * S.GetRoster().Num() + RoomUpkeep);

	// Gear: Gadget slot & tier-2 gear need the Workshop.
	const int32 CId = S.GetRoster()[0].Id;
	const FDCGearDef* Gadget1 = TA::Data(S).Gear.FindByPredicate([](const FDCGearDef& G) { return G.Item.Slot == EDCGearSlot::Gadget && G.Item.Tier == 1; });
	const FDCGearDef* Tier2 = TA::Data(S).Gear.FindByPredicate([](const FDCGearDef& G) { return G.Item.Tier == 2 && G.Item.Slot != EDCGearSlot::Gadget; });
	const FDCGearDef* Tool1 = TA::Data(S).Gear.FindByPredicate([](const FDCGearDef& G) { return G.Item.Tier == 1 && G.Item.Slot == EDCGearSlot::Tool; });
	if (Gadget1) { TestFalse(TEXT("no gadget slot yet"), S.BuyGear(Gadget1->Item.Id, CId)); }
	if (Tier2) { TestFalse(TEXT("no tier-2 yet"), S.BuyGear(Tier2->Item.Id, CId)); }
	if (Tool1)
	{
		const FDCStatBlock Before = S.GetEffectiveStats(CId);
		const int32 Cash0 = S.GetCash();
		TestTrue(TEXT("buy tool"), S.BuyGear(Tool1->Item.Id, CId));
		TestEqual(TEXT("paid"), Cash0 - S.GetCash(), Tool1->Item.Price);
		const FDCStatBlock After = S.GetEffectiveStats(CId);
		for (int32 i = 0; i < (int32)EDCStat::MAX; ++i)
		{
			TestEqual(TEXT("gear stat"), After.Get((EDCStat)i), FMath::Max(1, Before.Get((EDCStat)i) + Tool1->Item.StatBonus.Get((EDCStat)i)));
		}
	}
	TestTrue(TEXT("build workshop"), S.BuildRoom(EDCRoomType::Workshop));
	if (Gadget1) { TestTrue(TEXT("gadget slot with workshop"), S.BuyGear(Gadget1->Item.Id, CId)); }
	if (Tier2) { TestTrue(TEXT("tier-2 with workshop"), S.BuyGear(Tier2->Item.Id, CId)); }
	TA::SetRoom(S, EDCRoomType::Workshop, 3);
	if (Tool1)
	{
		const FDCGearItem* Cat = S.GetGearCatalogue().FindByPredicate([Tool1](const FDCGearItem& G) { return G.Id == Tool1->Item.Id; });
		TestEqual(TEXT("workshop L3 discount"), Cat ? Cat->Price : -1, FDCSimImpl::RoundTo(Tool1->Item.Price * 0.8f, 10));
	}

	// Garage: crews per night, heavy jobs, wheelman bonus.
	TestEqual(TEXT("1 crew"), S.GetMaxCrewsPerNight(), 1);
	TA::Heists(S) = 12;
	TestTrue(TEXT("build garage"), S.BuildRoom(EDCRoomType::Garage));
	TestEqual(TEXT("2 crews"), S.GetMaxCrewsPerNight(), 2);
	bool bSawHeavy = false;
	for (int32 i = 0; i < 40 && !bSawHeavy; ++i) { TA::Regenerate(S); bSawHeavy = S.GetJobs().ContainsByPredicate([](const FDCJob& J) { return J.bHeavy; }); }
	TestTrue(TEXT("heavy jobs appear with garage"), bSawHeavy);
	TA::SetRoom(S, EDCRoomType::Garage, 0);
	for (int32 i = 0; i < 40; ++i)
	{
		TA::Regenerate(S);
		if (S.GetJobs().ContainsByPredicate([](const FDCJob& J) { return J.bHeavy; })) { AddError(TEXT("heavy job without garage")); break; }
	}
	TA::SetRoom(S, EDCRoomType::Garage, 1);
	TA::Roster(S).Reset();
	TA::Jobs(S).Reset();
	const int32 WId = TA::AddCriminal(S, EDCCriminalType::Wheelman, 40).Id;
	FDCJob& WJ = TA::AddJob(S, { EDCCriminalType::Wheelman }, 40.f);
	FDCCrewPlan P; P.JobId = WJ.Id; P.CriminalIds = { WId }; P.Cut = B.FairCutBase;
	FDCSuccessBreakdown Bd = S.PreviewPlan(P);
	TestEqual(TEXT("garage wheelman bonus"), Bd.ModifierBonus, 0.04f, 1e-4f);
	TestTrue(TEXT("modifier line"), Bd.Lines.ContainsByPredicate([](const FDCBreakdownLine& L) { return L.Label.Contains(TEXT("Garage")); }));

	// Perks.
	const int32 FId = TA::AddCriminal(S, EDCCriminalType::Face, 40).Id;
	const int32 Charm0 = S.GetEffectiveStats(FId).Charm;
	TA::AddPerk(S, TEXT("method_acting"));
	TestEqual(TEXT("method acting"), S.GetEffectiveStats(FId).Charm, Charm0 + 10);
	TestEqual(TEXT("method acting is Face-only"), S.GetEffectiveStats(WId).Charm, 40);
	const int32 Luck0 = S.GetEffectiveLuck(WId);
	TA::AddPerk(S, TEXT("lucky_charm"));
	TestEqual(TEXT("lucky charm"), S.GetEffectiveLuck(WId), Luck0 + 1);
	const int32 Up0 = S.GetUpkeepDue();
	TA::AddPerk(S, TEXT("shoestring_budget"));
	TestEqual(TEXT("shoestring upkeep"), S.GetUpkeepDue(), FMath::RoundToInt(Up0 * 0.75f));
	const int32 Pay0 = S.PreviewPlan(P).Payout;
	TA::AddPerk(S, TEXT("hot_set"));
	TestEqual(TEXT("payout mult stacks multiplicatively"), S.PreviewPlan(P).Payout, FDCSimImpl::RoundTo(WJ.Payout * 0.9f * 1.3f, 100));
	TestTrue(TEXT("payout changed"), S.PreviewPlan(P).Payout != Pay0);
	TA::AddPerk(S, TEXT("ensemble_cast"));
	P.CriminalIds = { WId, FId };
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("ensemble + garage"), Bd.ModifierBonus, 0.04f + 0.03f, 1e-4f);
	TA::AddPerk(S, TEXT("typecasting"));
	Bd = S.PreviewPlan(P);
	TestEqual(TEXT("typecasting"), Bd.ModifierBonus, 0.04f + 0.03f + 0.06f, 1e-4f);

	// Lawyer: arrest multiplier.
	const float Arrest0 = S.PreviewPlan(P).ArrestChanceOnFail;
	TA::SetRoom(S, EDCRoomType::LawyerOffice, 1);
	TestEqual(TEXT("lawyer arrest mult"), S.PreviewPlan(P).ArrestChanceOnFail, Arrest0 * 0.85f, 1e-4f);

	// Clinic: patch-up cost.
	TA::Roster(S)[0].Status = EDCCriminalStatus::Injured;
	TA::Roster(S)[0].InjuredNights = 2;
	TestEqual(TEXT("no clinic -> no patch-up"), S.GetPatchUpCost(WId), -1);
	TA::SetRoom(S, EDCRoomType::Clinic, 2);
	TestEqual(TEXT("clinic L2 patch-up"), S.GetPatchUpCost(WId), 3000);
	TestTrue(TEXT("patch up"), S.PatchUp(WId));
	TestEqual(TEXT("healed"), S.FindCriminal(WId)->Status, EDCCriminalStatus::Available);

	// Board size: room sets, perk adds (after night 1).
	TA::PerksTaken(S).Reset();
	TA::SetRoom(S, EDCRoomType::Office, 2);
	TA::Cash(S) = 1000000;
	S.EndNight(); S.FinishDailies(); // night 2
	TestEqual(TEXT("office L2 board"), S.GetJobs().FilterByPredicate([](const FDCJob& J) { return !J.bFinalCut; }).Num(), 4);
	TA::AddPerk(S, TEXT("location_scout"));
	TA::Regenerate(S);
	TestEqual(TEXT("location scout +1"), S.GetJobs().FilterByPredicate([](const FDCJob& J) { return !J.bFinalCut; }).Num(), 5);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimNightRulesTest, "DirectorsCut.Sim.NightRules", DCSimFlags)
bool FDCSimNightRulesTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim(true);
	UDCRunSubsystem& S = *SP;

	// Game over when broke.
	{
		S.StartNewRun(3);
		bool bGameOver = false;
		FDCRunSummary Sum;
		S.OnGameOver.AddLambda([&](const FDCRunSummary& X) { bGameOver = true; Sum = X; });
		TA::Cash(S) = 10;
		S.EndNight();
		TestTrue(TEXT("report game over"), S.GetLastNightReport().bGameOver);
		TestEqual(TEXT("still dailies"), S.GetPhase(), EDCGamePhase::Dailies);
		S.FinishDailies();
		TestEqual(TEXT("phase game over"), S.GetPhase(), EDCGamePhase::GameOver);
		TestTrue(TEXT("OnGameOver"), bGameOver);
		TestFalse(TEXT("end reason"), Sum.EndReason.IsEmpty());
		TestEqual(TEXT("summary seed"), Sum.Seed, 3);
		TestTrue(TEXT("EndNight refused after game over"), S.EndNight().Num() == 0 && S.GetPhase() == EDCGamePhase::GameOver);
		S.OnGameOver.Clear();
	}
	// Game over: no crew and can't afford a recruit.
	{
		S.StartNewRun(4);
		TA::Roster(S).Reset();
		TA::Cash(S) = 100000;
		S.EndNight();
		TestFalse(TEXT("no crew but can afford: alive"), S.GetLastNightReport().bGameOver);
		S.FinishDailies();
		TA::Cash(S) = 1;
		S.EndNight();
		TestTrue(TEXT("no crew, no cash: game over"), S.GetLastNightReport().bGameOver);
	}
	// Perk offer after perkEveryNHeists; visible + broadcast after FinishDailies.
	{
		S.StartNewRun(6);
		TArray<FDCPerk> Offered;
		S.OnPerkOffered.AddLambda([&](const TArray<FDCPerk>& O) { Offered = O; });
		TA::Heists(S) = TA::Data(S).Balance.PerkEveryNHeists;
		TA::Cash(S) = 100000;
		S.EndNight();
		TestTrue(TEXT("report perk offered"), S.GetLastNightReport().bPerkOffered);
		TestTrue(TEXT("pending"), S.HasPendingPerkOffer());
		S.FinishDailies();
		TestEqual(TEXT("broadcast 3"), Offered.Num(), TA::Data(S).Balance.PerkOfferSize);
		TSet<FName> Ids; for (const FDCPerk& P : Offered) { Ids.Add(P.Id); }
		TestEqual(TEXT("distinct"), Ids.Num(), Offered.Num());
		TestFalse(TEXT("pick unknown"), S.PickPerk(TEXT("nope")));
		TestTrue(TEXT("pick"), S.PickPerk(Offered[1].Id));
		TestEqual(TEXT("taken"), S.GetPerksTaken().Num(), 1);
		TestFalse(TEXT("offer cleared"), S.HasPendingPerkOffer());
		TA::Cash(S) = 100000;
		S.EndNight(); S.FinishDailies();
		TestFalse(TEXT("no second offer for same block"), S.HasPendingPerkOffer());
		TestEqual(TEXT("tier unlocked"), S.GetUnlockedTier(), 2);
		S.OnPerkOffered.Clear();
	}
	// Retakes: Screening Room re-shoots a failed heist.
	{
		bool bDone = false;
		for (int32 Seed = 10; Seed < 60 && !bDone; ++Seed)
		{
			CleanSlate(S, Seed);
			TA::SetRoom(S, EDCRoomType::ScreeningRoom, 1);
			const int32 MId = TA::AddCriminal(S, EDCCriminalType::Muscle, 30).Id;
			FDCJob& J = TA::AddJob(S, { EDCCriminalType::Muscle }, 100000.f);
			TA::RollSeeds(S)[J.Id] = Seed;
			FDCCrewPlan P; P.JobId = J.Id; P.CriminalIds = { MId }; P.Cut = 0.3f;
			TestTrue(TEXT("commit hopeless job"), S.CommitPlan(P));
			const TArray<FDCHeistResult> Res = S.EndNight();
			if (Res.Num() != 1 || (Res[0].Outcome != EDCHeistOutcome::Failed && Res[0].Outcome != EDCHeistOutcome::Busted)) { S.FinishDailies(); continue; }
			bDone = true;
			TestTrue(TEXT("retake offered"), Res[0].bRetakeAvailable);
			TestEqual(TEXT("1 retake"), S.GetRetakesRemaining(), 1);
			FDCHeistResult New;
			bool bEvent = false;
			S.OnRetakeResolved.AddLambda([&](const FDCHeistResult&) { bEvent = true; });
			TestTrue(TEXT("use retake"), S.UseRetake(Res[0].ResultId, &New));
			TestTrue(TEXT("retaken"), New.bRetaken);
			TestTrue(TEXT("retake event"), bEvent);
			TestEqual(TEXT("first roll kept"), New.FirstRoll, Res[0].FirstRoll);
			TestEqual(TEXT("0 retakes"), S.GetRetakesRemaining(), 0);
			TestFalse(TEXT("no more retakes"), S.UseRetake(Res[0].ResultId));
			TestEqual(TEXT("report updated"), S.GetLastNightReport().Results[0].bRetaken, true);
			S.OnRetakeResolved.Clear();
		}
		TestTrue(TEXT("found a failed heist to retake"), bDone);
	}
	// Arrest: gear lost; Lawyer bail within one night.
	{
		bool bDone = false;
		for (int32 Seed = 100; Seed < 400 && !bDone; ++Seed)
		{
			CleanSlate(S, Seed);
			TA::SetRoom(S, EDCRoomType::LawyerOffice, 1);
			FDCCriminal& C = TA::AddCriminal(S, EDCCriminalType::Muscle, 10);
			C.Equipped.Add(EDCGearSlot::Tool, TA::Data(S).Gear[0].Item.Id);
			const int32 MId = C.Id;
			FDCJob& J = TA::AddJob(S, { EDCCriminalType::Muscle }, 100000.f);
			J.ArrestRisk = 0.9f;
			TA::RollSeeds(S)[J.Id] = Seed;
			FDCCrewPlan P; P.JobId = J.Id; P.CriminalIds = { MId }; P.Cut = 0.3f;
			S.CommitPlan(P);
			const TArray<FDCHeistResult> Res = S.EndNight();
			if (Res.Num() != 1 || Res[0].Members.Num() != 1 || !Res[0].Members[0].bArrested) { S.FinishDailies(); continue; }
			bDone = true;
			TestNull(TEXT("gone from roster"), S.FindCriminal(MId));
			TestEqual(TEXT("in fallen"), S.GetFallen().Num(), 1);
			TestEqual(TEXT("gear lost"), S.GetFallen()[0].Equipped.Num(), 0);
			TestEqual(TEXT("status arrested"), S.GetFallen()[0].Status, EDCCriminalStatus::Arrested);
			S.FinishDailies();
			TestTrue(TEXT("bail cost"), S.GetBailCost(MId) > 0);
			TestTrue(TEXT("bail"), S.BailOut(MId));
			TestNotNull(TEXT("back in roster"), S.FindCriminal(MId));
		}
		TestTrue(TEXT("found an arrest"), bDone);
	}
	// Levelling: XP resets on level up; stats/luck grow.
	{
		CleanSlate(S, 1);
		FDCCriminal& C = TA::AddCriminal(S, EDCCriminalType::Hacker, 40, 50, 0);
		const int32 Tech0 = C.BaseStats.Tech;
		int32 Levels = 0;
		S.OnCriminalLevelUp.AddLambda([&](int32, int32) { ++Levels; });
		FDCSimImpl::AddXP(S, C, C.XPToNext + 5, nullptr);
		TestEqual(TEXT("level 2"), C.Level, 2);
		TestEqual(TEXT("xp reset"), C.XP, 0);
		TestEqual(TEXT("primary +4"), C.BaseStats.Tech, Tech0 + TA::Data(S).Balance.LevelUpPrimary);
		TestEqual(TEXT("luck +1 on even level"), C.Luck, 1);
		TestEqual(TEXT("xp to next"), C.XPToNext, DCSimFormula::XPToNext(2, 100.f, 1.4f));
		TestEqual(TEXT("delegate"), Levels, 1);
		for (int32 i = 0; i < 30; ++i) { FDCSimImpl::AddXP(S, C, 100000, nullptr); }
		TestEqual(TEXT("max level"), C.Level, TA::Data(S).Balance.MaxLevel);
		S.OnCriminalLevelUp.Clear();
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimWinnableRunTest, "DirectorsCut.Sim.WinnableRun", DCSimFlags)
bool FDCSimWinnableRunTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim();
	UDCRunSubsystem& S = *SP;
	S.StartNewRun(777);
	bool bWonEvent = false;
	FDCRunSummary Sum;
	S.OnRunWon.AddLambda([&](const FDCRunSummary& X) { bWonEvent = true; Sum = X; });

	// Script: play honestly for a few nights, then cheat cash/heists to reach the Final Cut.
	for (int32 N = 0; N < 5 && S.GetPhase() == EDCGamePhase::Planning; ++N) { TA::Cash(S) += 20000; BotPlan(S); S.EndNight(); S.FinishDailies(); }
	TestEqual(TEXT("still planning"), S.GetPhase(), EDCGamePhase::Planning);
	TA::Heists(S) = FMath::Max(S.GetHeistsCompleted(), TA::Data(S).Balance.FinalCutRequiredHeists);
	TA::Cash(S) = 500000;
	if (!S.IsRoomBuilt(EDCRoomType::Lounge)) { TestTrue(TEXT("build lounge"), S.BuildRoom(EDCRoomType::Lounge)); }
	while (S.GetRoom(EDCRoomType::Lounge).Level < 3) { if (!TestTrue(TEXT("upgrade lounge"), S.UpgradeRoom(EDCRoomType::Lounge))) { break; } }
	TestTrue(TEXT("build screening room"), S.BuildRoom(EDCRoomType::ScreeningRoom));
	const FDCJob* Final = S.GetJobs().FindByPredicate([](const FDCJob& J) { return J.bFinalCut; });
	TestNotNull(TEXT("final cut on board after unlock"), Final);

	for (int32 N = 0; N < 40 && S.GetPhase() == EDCGamePhase::Planning; ++N)
	{
		TA::Cash(S) = 500000;
		if (S.HasPendingPerkOffer()) { S.PickPerk(S.GetPerkOffer()[0].Id); }
		// Keep a strong cast of every required type.
		for (EDCCriminalType T : { EDCCriminalType::Face, EDCCriminalType::Ghost, EDCCriminalType::Safecracker, EDCCriminalType::Hacker, EDCCriminalType::Wheelman, EDCCriminalType::Muscle })
		{
			if (!S.GetRoster().ContainsByPredicate([T](const FDCCriminal& C) { return C.Type == T && C.IsAvailable(); }))
			{
				TA::AddCriminal(S, T, 90, 80, 5);
			}
		}
		Final = S.GetJobs().FindByPredicate([](const FDCJob& J) { return J.bFinalCut; });
		if (!TestNotNull(TEXT("final cut always offered"), Final)) { break; }
		TSet<int32> Used;
		const FDCCrewPlan P = BuildCrew(S, *Final, 0.25f, Used);
		FString Err;
		if (!S.CommitPlan(P, &Err)) { AddInfo(TEXT("Final Cut not castable: ") + Err); }
		S.EndNight();
		for (const FDCHeistResult& R : TArray<FDCHeistResult>(S.GetLastNightReport().Results)) { if (R.bRetakeAvailable) { S.UseRetake(R.ResultId); } }
		S.FinishDailies();
	}
	TestEqual(TEXT("won"), S.GetPhase(), EDCGamePhase::Won);
	TestTrue(TEXT("OnRunWon"), bWonEvent);
	TestTrue(TEXT("summary won"), Sum.bWon);
	TestTrue(TEXT("credits starring"), Sum.Starring.Num() > 0);
	TestTrue(TEXT("rooms built"), Sum.RoomsBuilt >= 2);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimAutoPlayTest, "DirectorsCut.Sim.AutoPlay200", DCSimFlags)
bool FDCSimAutoPlayTest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> SP = NewSim();
	UDCRunSubsystem& S = *SP;
	int32 Seed = 2026;
	S.StartNewRun(Seed);
	int32 Runs = 1, Wins = 0, Losses = 0, Heists = 0, Attempts = 0, MaxNight = 0, Problems = 0;
	int32 StateChanges = 0;
	S.OnRunStateChanged.AddLambda([&]() { ++StateChanges; });
	auto Check = [&](bool bOk, const FString& Msg) { if (!bOk && Problems++ < 20) { AddError(Msg); } };

	for (int32 N = 0; N < 200; ++N)
	{
		if (S.GetPhase() == EDCGamePhase::GameOver || S.GetPhase() == EDCGamePhase::Won)
		{
			(S.GetPhase() == EDCGamePhase::Won ? Wins : Losses)++;
			S.StartNewRun(++Seed);
			++Runs;
		}
		BotPlan(S);
		for (const FDCCrewPlan& P : S.GetCommittedPlans())
		{
			const FDCSuccessBreakdown Bd = S.PreviewPlan(P);
			Check(Bd.bValid, TEXT("committed plan invalid: ") + Bd.InvalidReason);
			for (float V : { Bd.SuccessChance, Bd.SkillChance, Bd.CrewScore, Bd.Margin, Bd.WillFactor, Bd.LuckyBreakChance, Bd.ArrestChanceOnFail })
			{
				Check(FMath::IsFinite(V), TEXT("NaN/Inf in breakdown"));
			}
			Check(Bd.SuccessChance >= 0.f && Bd.SuccessChance <= 1.f, TEXT("success out of range"));
			for (const FDCBreakdownLine& L : Bd.Lines) { Check(FMath::IsFinite(L.Value), TEXT("NaN line ") + L.Label); }
		}
		const int32 Changes0 = StateChanges;
		const TArray<FDCHeistResult> Res = S.EndNight();
		Check(StateChanges > Changes0, TEXT("EndNight broadcast"));
		for (const FDCHeistResult& R : Res)
		{
			++Attempts;
			if (R.Outcome == EDCHeistOutcome::Clean || R.Outcome == EDCHeistOutcome::Messy) { ++Heists; }
			Check(R.Outcome != EDCHeistOutcome::None, TEXT("no outcome"));
			Check(FMath::IsFinite(R.Roll) && R.Roll >= 0.f && R.Roll < 1.f, TEXT("bad roll"));
			Check(!R.FlavourLine.Contains(TEXT("{")), TEXT("unsubstituted flavour: ") + R.FlavourLine);
		}
		for (const FDCHeistResult& R : TArray<FDCHeistResult>(S.GetLastNightReport().Results)) { if (R.bRetakeAvailable) { S.UseRetake(R.ResultId); } }
		S.FinishDailies();
		MaxNight = FMath::Max(MaxNight, S.GetNight());
		Check(S.GetRoster().Num() <= S.GetRosterCap(), TEXT("roster over cap"));
		for (const FDCCriminal& C : S.GetRoster())
		{
			Check(C.Level >= 1 && C.Level <= 10 && C.Loyalty >= 0 && C.Loyalty <= 100 && C.Luck >= 0 && C.Luck <= 10, TEXT("criminal out of range"));
			Check(C.Status != EDCCriminalStatus::OnJob, TEXT("stuck OnJob after night"));
			Check(C.Status != EDCCriminalStatus::Injured || C.InjuredNights > 0, TEXT("injured with 0 nights"));
		}
		if (S.GetPhase() == EDCGamePhase::Planning) { Check(S.GetCash() >= 0, TEXT("negative cash while planning")); }
	}
	AddInfo(FString::Printf(TEXT("AutoPlay: %d runs, %d won, %d lost, %d/%d heists succeeded, max night %d"), Runs, Wins, Losses, Heists, Attempts, MaxNight));
	UE_LOG(LogDC, Display, TEXT("[SimTest] AutoPlay: %d runs, %d won, %d lost, %d/%d heists succeeded, max night %d"), Runs, Wins, Losses, Heists, Attempts, MaxNight);
	TestTrue(TEXT("some heists attempted"), Attempts > 50);
	TestEqual(TEXT("problems"), Problems, 0);
	return true;
}

#endif
