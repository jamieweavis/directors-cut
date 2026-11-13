// Director's Cut — simulation automation tests: DirectorsCut.Sim.* (Agent A). Fixture data, separate save slot.
#include "Misc/AutomationTest.h"
#include "Tests/Sim/DCSimTestUtils.h"
#include "DirectorsCut.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

using A = FDCSimTestAccess;
constexpr EAutomationTestFlags DCSimFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// ─── data ───────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimDataLoad, "DirectorsCut.Sim.DataLoad", DCSimFlags)
bool FDCSimDataLoad::RunTest(const FString&)
{
	FDCSimData D;
	TestTrue(TEXT("fixtures load"), D.Load(FDCSimData::FixturesDir()));
	TestEqual(TEXT("no warnings in fixtures"), D.Warnings, 0);
	TestTrue(TEXT(">=10 missions"), D.Missions.Num() >= 10);
	TestNotNull(TEXT("big score"), D.FindTemplate(TEXT("the_big_score")));
	TestTrue(TEXT("big score final"), D.FindTemplate(TEXT("the_big_score")) && D.FindTemplate(TEXT("the_big_score"))->bFinal);
	TestEqual(TEXT("8 rooms"), D.Rooms.Num(), 8);
	TestEqual(TEXT("6 types"), D.Types.Num(), 6);
	TestEqual(TEXT("threat points"), D.Balance.ThreatPoints, 1.5f);
	// Default loader: Content/Data is either v2 or we fall back to fixtures; either way it must load.
	FDCSimData D2;
	TestTrue(TEXT("default dir loads"), D2.Load(FString()));
	return true;
}

// ─── success formula ────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimFormula, "DirectorsCut.Sim.Formula", DCSimFlags)
bool FDCSimFormula::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(42);
	A::ClearRun(*S);
	const FDCCriminal& H = A::AddCriminal(*S, EDCCriminalType::Hacker, 3, { TEXT("loop_feed") });
	const FDCCriminal& Mu = A::AddCriminal(*S, EDCCriminalType::Muscle, 1, { TEXT("brawler") }, { TEXT("hothead") });
	const int32 HId = H.Id, MId = Mu.Id;
	FDCMission& M = A::AddMission(*S, 3, 2, { EDCThreat::Cameras, EDCThreat::Guards, EDCThreat::Vault }, 200.f, 500, 10000);
	const int32 MisId = M.Id;
	TestFalse(TEXT("day at 07:00"), S->IsNight());

	FDCCrewPlan P; P.MissionId = MisId; P.CriminalIds = { HId, MId }; P.Cut = 0.3f;
	FDCSuccessBreakdown B = S->PreviewPlan(P);
	// Points = 2 + 1.5*3 = 6.5; LF: 1.0 + 0.5 (2 under); countered 2 -> 3.0; hothead -0.05.
	TestTrue(TEXT("valid"), B.bValid);
	TestEqual(TEXT("crew chance"), B.CrewChance, 1.5f / 6.5f, 1e-4f);
	TestEqual(TEXT("threat chance"), B.ThreatChance, 3.f / 6.5f, 1e-4f);
	TestEqual(TEXT("modifier chance"), B.ModifierChance, -0.05f, 1e-4f);
	TestEqual(TEXT("will factor"), B.WillFactor, 1.f, 1e-4f);
	TestEqual(TEXT("success"), B.SuccessChance, 1.5f / 6.5f + 3.f / 6.5f - 0.05f, 1e-4f);
	TestEqual(TEXT("member factor H"), B.MemberFactor.FindRef(HId), 1.f, 1e-4f);
	TestEqual(TEXT("member factor M"), B.MemberFactor.FindRef(MId), 0.5f, 1e-4f);
	TestEqual(TEXT("3 threat slots"), B.Threats.Num(), 3);
	if (B.Threats.Num() == 3)
	{
		TestEqual(TEXT("cameras by hacker"), B.Threats[0].CounteredBy, HId);
		TestEqual(TEXT("cameras source"), B.Threats[0].Source, FName(TEXT("loop_feed")));
		TestEqual(TEXT("guards by muscle"), B.Threats[1].CounteredBy, MId);
		TestFalse(TEXT("vault open"), B.Threats[2].IsCountered());
	}
	TestEqual(TEXT("one modifier"), B.Modifiers.Num(), 1);
	if (B.Modifiers.Num() == 1) { TestEqual(TEXT("hothead hit"), B.Modifiers[0].Id, FName(TEXT("hothead"))); TestTrue(TEXT("negative"), B.Modifiers[0].bNegative); }
	TestEqual(TEXT("cost"), B.Cost, 500);
	TestEqual(TEXT("payout"), B.Payout, 10000);
	TestEqual(TEXT("take"), B.DirectorTake, 3000);
	TestEqual(TEXT("duration"), B.Duration, 200.f, 1e-3f);

	// Greedy cut: will = 1 - 3*(0.5-0.3) = 0.4 < 0.45 -> both refuse; WF = 0.6 + 0.4*0.4 = 0.76.
	P.Cut = 0.5f;
	B = S->PreviewPlan(P);
	TestEqual(TEXT("will"), B.Willingness.FindRef(HId), 0.4f, 1e-4f);
	TestEqual(TEXT("refusers"), B.RefusingIds.Num(), 2);
	TestEqual(TEXT("greedy WF"), B.WillFactor, 0.76f, 1e-4f);
	TestEqual(TEXT("greedy success"), B.SuccessChance, (1.5f / 6.5f + 3.f / 6.5f - 0.05f) * 0.76f, 1e-4f);
	TestEqual(TEXT("block refusal"), B.Block, EDCPlanBlock::Refusal);
	// Loyalty raises the fair cut: loyalty 100 -> fair 0.4 -> will 0.7, no refusal.
	for (FDCCriminal& C : A::Roster(*S)) { C.Loyalty = 100; }
	B = S->PreviewPlan(P);
	TestEqual(TEXT("loyal will"), B.Willingness.FindRef(HId), 0.7f, 1e-4f);
	TestEqual(TEXT("no refusers"), B.RefusingIds.Num(), 0);
	// willMin perk floors willingness.
	for (FDCCriminal& C : A::Roster(*S)) { C.Loyalty = 50; }
	A::AddPerk(*S, TEXT("loyal_crew"));
	B = S->PreviewPlan(P);
	TestEqual(TEXT("willMin perk"), B.Willingness.FindRef(HId), 0.6f, 1e-4f);
	A::PerksTaken(*S).Reset();

	// Durations with traits: Lead Foot 0.75 x Slow 1.15; with Fast Cars 0.85 -> clamp at 0.5 is not hit.
	A::Roster(*S)[0].Traits.Add(TEXT("lead_foot"));
	A::Roster(*S)[1].Traits.Add(TEXT("slow"));
	P.Cut = 0.3f;
	B = S->PreviewPlan(P);
	TestEqual(TEXT("trait duration"), B.Duration, 200.f * 0.75f * 1.15f, 1e-2f);
	A::AddPerk(*S, TEXT("fast_cars"));
	B = S->PreviewPlan(P);
	TestEqual(TEXT("perk duration"), B.Duration, 200.f * 0.75f * 1.15f * 0.85f, 1e-2f);
	A::Roster(*S)[0].Traits.Add(TEXT("lead_foot")); // duplicate innate traits stack
	A::Roster(*S)[1].Traits.Remove(TEXT("slow"));
	A::Roster(*S)[1].Traits.Add(TEXT("lead_foot"));
	B = S->PreviewPlan(P);
	TestEqual(TEXT("min duration clamp"), B.Duration, 100.f, 1e-2f);

	// Block ordering.
	FDCCrewPlan E; E.MissionId = MisId; E.CriminalIds = { HId };
	TestEqual(TEXT("slots empty"), S->PreviewPlan(E).Block, EDCPlanBlock::SlotsEmpty);
	E.CriminalIds = { HId, HId };
	TestEqual(TEXT("dup unavailable"), S->PreviewPlan(E).Block, EDCPlanBlock::CrewUnavailable);
	E.MissionId = 999999;
	TestEqual(TEXT("expired"), S->PreviewPlan(E).Block, EDCPlanBlock::Expired);
	A::Cash(*S) = 100;
	TestEqual(TEXT("no cash"), S->PreviewPlan(P).Block, EDCPlanBlock::NoCash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimLevelFactor, "DirectorsCut.Sim.LevelFactor", DCSimFlags)
bool FDCSimLevelFactor::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(1);
	TestEqual(TEXT("equal"), FDCSimImpl::LevelFactor(*S, 5.f, 5), 1.f, 1e-4f);
	TestEqual(TEXT("+2"), FDCSimImpl::LevelFactor(*S, 7.f, 5), 1.1f, 1e-4f);
	TestEqual(TEXT("+0.5"), FDCSimImpl::LevelFactor(*S, 5.5f, 5), 1.025f, 1e-4f);
	TestEqual(TEXT("cap"), FDCSimImpl::LevelFactor(*S, 10.f, 5), 1.2f, 1e-4f);
	TestEqual(TEXT("-1"), FDCSimImpl::LevelFactor(*S, 4.f, 5), 0.75f, 1e-4f);
	TestEqual(TEXT("-0.5 rounds up"), FDCSimImpl::LevelFactor(*S, 4.5f, 5), 0.75f, 1e-4f);
	TestEqual(TEXT("-1.5"), FDCSimImpl::LevelFactor(*S, 3.5f, 5), 0.5f, 1e-4f);
	TestEqual(TEXT("-3"), FDCSimImpl::LevelFactor(*S, 2.f, 5), 0.1f, 1e-4f);
	TestEqual(TEXT("-6"), FDCSimImpl::LevelFactor(*S, 1.f, 7), 0.1f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimCounters, "DirectorsCut.Sim.Counters", DCSimFlags)
bool FDCSimCounters::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(7);
	A::ClearRun(*S);
	A::SetRoom(*S, EDCRoomType::Workshop, 1); // 3 gear slots
	const int32 Hi = A::AddCriminal(*S, EDCCriminalType::Hacker, 5, { TEXT("loop_feed") }).Id;
	const int32 Lo = A::AddCriminal(*S, EDCCriminalType::Ghost, 2, { TEXT("blind_spot") }).Id;
	A::Roster(*S)[1].Equipped.Add(EDCGearSlot::Tool, TEXT("camera_spoofer"));
	const int32 Mid = A::AddMission(*S, 3, 2, { EDCThreat::Cameras, EDCThreat::Cameras, EDCThreat::Cameras, EDCThreat::Locks }).Id;
	FDCCrewPlan P; P.MissionId = Mid; P.CriminalIds = { Hi, Lo };
	const FDCSuccessBreakdown B = S->PreviewPlan(P);
	TestEqual(TEXT("first cameras -> lowest level"), B.Threats[0].CounteredBy, Lo);
	TestEqual(TEXT("second cameras -> lowest level again (gear)"), B.Threats[1].CounteredBy, Lo);
	TestEqual(TEXT("gear source"), B.Threats[1].Source, FName(TEXT("camera_spoofer")));
	TestEqual(TEXT("third cameras -> hacker"), B.Threats[2].CounteredBy, Hi);
	TestFalse(TEXT("locks open"), B.Threats[3].IsCountered());
	TestEqual(TEXT("counters incl. gear"), S->GetCounters(Lo).Num(), 2);

	// SuggestPlan prefers counters over raw level.
	A::AddCriminal(*S, EDCCriminalType::Muscle, 9, { TEXT("brawler") });
	const int32 Safe = A::AddCriminal(*S, EDCCriminalType::Safecracker, 1, { TEXT("lockpick") }).Id;
	const int32 M2 = A::AddMission(*S, 1, 1, { EDCThreat::Locks }).Id;
	const FDCCrewPlan SP = S->SuggestPlan(M2, 0.3f);
	TestEqual(TEXT("suggest size"), SP.CriminalIds.Num(), 1);
	if (SP.CriminalIds.Num() == 1) { TestEqual(TEXT("suggest picks the counter"), SP.CriminalIds[0], Safe); }
	return true;
}

// ─── lifecycle ──────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimLifecycle, "DirectorsCut.Sim.MissionLifecycle", DCSimFlags)
bool FDCSimLifecycle::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(11);
	TestEqual(TEXT("board full"), S->GetMissions().Num(), 4);
	TestEqual(TEXT("roster"), S->GetRoster().Num(), 4);
	TestEqual(TEXT("recruits"), S->GetRecruits().Num(), 3);
	int32 Doable = 0;
	for (const FDCMission& M : S->GetMissions()) { if (S->PreviewPlan(S->SuggestPlan(M.Id)).SuccessChance >= 0.6f) { ++Doable; } }
	TestTrue(TEXT(">=2 starting missions at 60%+"), Doable >= 2);

	A::ClearRun(*S);
	const int32 C1 = A::AddCriminal(*S, EDCCriminalType::Hacker, 3, { TEXT("loop_feed") }).Id;
	const int32 Mid = A::AddMission(*S, 1, 1, { EDCThreat::Cameras }, 100.f, 200, 4000).Id;
	A::Cash(*S) = 1000;
	int32 Started = 0, Completed = 0, Changes = 0;
	S->OnMissionStarted.AddLambda([&](const FDCMission&) { ++Started; });
	S->OnMissionCompleted.AddLambda([&](const FDCMissionReport&) { ++Completed; });
	S->OnRunStateChanged.AddLambda([&]() { ++Changes; });
	FDCCrewPlan P; P.MissionId = Mid; P.CriminalIds = { C1 }; P.Cut = 0.3f;
	EDCPlanBlock Block;
	TestTrue(TEXT("start"), S->StartMission(P, &Block));
	TestEqual(TEXT("cost paid"), S->GetCash(), 800);
	TestEqual(TEXT("on mission"), S->FindCriminal(C1)->Status, EDCCriminalStatus::OnMission);
	TestEqual(TEXT("active"), S->GetActiveMissionCount(), 1);
	TestFalse(TEXT("can't double book"), S->StartMission(P, &Block));
	S->AdvanceTime(50.0);
	TestEqual(TEXT("half progress"), S->GetMissionProgress(Mid), 0.5f, 1e-3f);
	TestEqual(TEXT("remaining"), S->GetMissionTimeRemaining(Mid), 50.0, 1e-3);
	const int32 ChangesBefore = Changes;
	S->AdvanceTime(10.0);
	TestEqual(TEXT("no change broadcast for plain time"), Changes, ChangesBefore);
	S->AdvanceTime(40.0);
	TestEqual(TEXT("completed"), Completed, 1);
	TestNull(TEXT("removed from board"), S->FindMission(Mid));
	TestEqual(TEXT("report"), S->GetReports().Num(), 1);
	const FDCMissionReport& R = S->GetReports()[0];
	TestEqual(TEXT("completed at end"), R.CompletedAt, 100.0, 1e-6);
	TestEqual(TEXT("crew back"), S->FindCriminal(C1)->ActiveMissionId, INDEX_NONE);
	if (R.Outcome == EDCMissionOutcome::Success) { TestEqual(TEXT("take"), S->GetCash(), 800 + R.DirectorTake); TestEqual(TEXT("take value"), R.DirectorTake, 1200); }
	TestEqual(TEXT("unseen"), S->GetUnseenReportCount(), 1);
	S->MarkAllReportsSeen();
	TestEqual(TEXT("seen"), S->GetUnseenReportCount(), 0);

	// Expiry + posting.
	FDCMission& Ex = A::AddMission(*S, 1, 1, { EDCThreat::Locks });
	Ex.ExpiresAt = S->GetRunTime() + 30.0;
	const int32 ExId = Ex.Id;
	S->AdvanceTime(29.0);
	TestNotNull(TEXT("still posted"), S->FindMission(ExId));
	S->AdvanceTime(2.0);
	TestNull(TEXT("expired"), S->FindMission(ExId));
	A::NextPostAt(*S) = S->GetRunTime() + 5.0;
	S->AdvanceTime(5.0);
	TestEqual(TEXT("new posting"), S->GetMissions().Num(), 1);
	S->AdvanceTime(40.0 * 3 + 1.0);
	TestEqual(TEXT("board fills at interval"), S->GetMissions().Num(), 4);
	S->AdvanceTime(200.0);
	TestTrue(TEXT("board never overfills"), S->GetMissions().Num() <= 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimPayday, "DirectorsCut.Sim.PaydayGameOver", DCSimFlags)
bool FDCSimPayday::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(5);
	TestEqual(TEXT("day 1"), S->GetDay(), 1);
	TestEqual(TEXT("07:00"), S->GetHourOfDay(), 7.f, 1e-3f);
	TestEqual(TEXT("payday in 23h"), S->GetSecondsUntilPayday(), 23.0 * 60.0, 1e-3);
	TestTrue(TEXT("sun up"), S->GetSunAlpha() > 0.f && S->GetSunAlpha() < 1.f);
	A::ClearRun(*S);
	A::AddCriminal(*S, EDCCriminalType::Muscle, 1, { TEXT("brawler") });
	A::AddCriminal(*S, EDCCriminalType::Ghost, 1, { TEXT("shadow") });
	A::SetRoom(*S, EDCRoomType::Lounge, 1);
	const int32 Due = S->GetUpkeepDue();
	TestEqual(TEXT("upkeep = crew + rooms"), Due, 2 * 300 + 150);
	A::Cash(*S) = 1000;
	int32 Paid = -1, PayDay = -1, DayStarted = -1;
	S->OnPayday.AddLambda([&](int32 D, int32 Amt) { PayDay = D; Paid = Amt; });
	S->OnDayStarted.AddLambda([&](int32 D) { DayStarted = D; });
	S->AdvanceTime(5.0 * 60.0); // 12:00
	TestEqual(TEXT("noon sun"), S->GetSunAlpha(), 1.f, 1e-3f);
	S->AdvanceTime(9.0 * 60.0); // 21:00
	TestTrue(TEXT("night"), S->IsNight());
	TestEqual(TEXT("night sun"), S->GetSunAlpha(), 0.f);
	S->AdvanceTime(9.0 * 60.0 - 1.0);
	TestEqual(TEXT("not yet paid"), Paid, -1);
	S->AdvanceTime(2.0);
	TestEqual(TEXT("paid"), Paid, Due);
	TestEqual(TEXT("payday day 2"), PayDay, 2);
	TestEqual(TEXT("day started"), DayStarted, 2);
	TestEqual(TEXT("cash"), S->GetCash(), 1000 - Due);
	TestEqual(TEXT("loyalty idle +3"), S->GetRoster()[0].Loyalty, 53);
	TestEqual(TEXT("still playing"), S->GetPhase(), EDCGamePhase::Playing);

	bool bOver = false;
	S->OnGameOver.AddLambda([&](const FDCRunSummary& Sum) { bOver = true; });
	S->AdvanceTime(24.0 * 60.0);
	TestTrue(TEXT("broke -> game over"), bOver);
	TestEqual(TEXT("phase"), S->GetPhase(), EDCGamePhase::GameOver);
	const double T = S->GetRunTime();
	S->AdvanceTime(100.0);
	TestEqual(TEXT("time frozen after game over"), S->GetRunTime(), T);

	// Empty roster with no money also ends the run.
	TStrongObjectPtr<UDCRunSubsystem> S2 = DCSimTest::NewSim();
	DCSimTest::FEndGuard G2(S2.Get());
	S2->StartNewRun(6);
	A::ClearRun(*S2);
	A::Cash(*S2) = 10;
	S2->AdvanceTime(1.0);
	TestEqual(TEXT("no crew no cash"), S2->GetPhase(), EDCGamePhase::GameOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimRooms, "DirectorsCut.Sim.Rooms", DCSimFlags)
bool FDCSimRooms::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(9);
	A::ClearRun(*S);
	A::AddCriminal(*S, EDCCriminalType::Muscle, 1, { TEXT("brawler") });
	TestTrue(TEXT("office built"), S->IsRoomBuilt(EDCRoomType::Office));
	FDCRoom L = S->GetRoom(EDCRoomType::Lounge);
	TestEqual(TEXT("locked by successes"), L.Block, EDCPlanBlock::Locked);
	TestEqual(TEXT("req"), L.RequiredSuccesses, 1);
	A::Successes(*S) = 1;
	A::Cash(*S) = 2000;
	TestEqual(TEXT("no cash"), S->GetRoom(EDCRoomType::Lounge).Block, EDCPlanBlock::NoCash);
	A::Cash(*S) = 10000;
	int32 Changed = 0;
	S->OnRoomChanged.AddLambda([&](const FDCRoom&) { ++Changed; });
	TestEqual(TEXT("roster cap"), S->GetRosterCap(), 4);
	TestTrue(TEXT("build"), S->BuildRoom(EDCRoomType::Lounge));
	TestEqual(TEXT("paid"), S->GetCash(), 7000);
	TestEqual(TEXT("building"), S->GetRoom(EDCRoomType::Lounge).State, EDCRoomState::Building);
	TestEqual(TEXT("event on start"), Changed, 1);
	EDCPlanBlock Block;
	TestFalse(TEXT("one at a time"), S->BuildRoom(EDCRoomType::Workshop, &Block));
	S->AdvanceTime(59.0);
	TestFalse(TEXT("not built yet"), S->IsRoomBuilt(EDCRoomType::Lounge));
	S->AdvanceTime(2.0);
	TestEqual(TEXT("built"), S->GetRoom(EDCRoomType::Lounge).State, EDCRoomState::Built);
	TestEqual(TEXT("level 1"), S->GetRoom(EDCRoomType::Lounge).Level, 1);
	TestEqual(TEXT("event on finish"), Changed, 2);
	TestEqual(TEXT("roster cap +2"), S->GetRosterCap(), 6);
	TestEqual(TEXT("upkeep shown"), S->GetRoom(EDCRoomType::Lounge).UpkeepPerDay, 150);

	// Gear availability + workshop.
	TestEqual(TEXT("2 slots"), S->GetGearSlotCount(), 2);
	const TArray<FDCGearItem> Cat = S->GetGearCatalogue();
	const FDCGearItem* Spoof = Cat.FindByPredicate([](const FDCGearItem& I) { return I.Id == TEXT("camera_spoofer"); });
	const FDCGearItem* Phone = Cat.FindByPredicate([](const FDCGearItem& I) { return I.Id == TEXT("burner_phone"); });
	const FDCGearItem* Rig = Cat.FindByPredicate([](const FDCGearItem& I) { return I.Id == TEXT("tactical_rig"); });
	TestTrue(TEXT("tier2 needs workshop"), Spoof && !Spoof->bAvailable && Spoof->UnavailableReason == TEXT("Workshop"));
	TestTrue(TEXT("tier3 needs workshop L3"), Rig && Rig->UnavailableReason == TEXT("Workshop L3"));
	TestTrue(TEXT("gadget slot needs workshop"), Phone && !Phone->bAvailable && Phone->UnavailableReason == TEXT("Workshop"));
	TestFalse(TEXT("no loot in shop"), Cat.ContainsByPredicate([](const FDCGearItem& I) { return I.Id == TEXT("gold_watch"); }));
	const int32 Cid = S->GetRoster()[0].Id;
	TestTrue(TEXT("buy cutters"), S->BuyGear(TEXT("bolt_cutters"), Cid));
	TestTrue(TEXT("counter from gear"), S->GetCounters(Cid).Contains(EDCThreat::Locks));
	TestTrue(TEXT("buy pepper (replaces)"), S->BuyGear(TEXT("pepper_spray"), Cid));
	TestEqual(TEXT("old to stash"), S->GetStash().Num(), 1);
	const int32 CashBefore = S->GetCash();
	TestTrue(TEXT("sell"), S->SellFromStash(TEXT("bolt_cutters")));
	TestEqual(TEXT("refund 50%"), S->GetCash(), CashBefore + 400);
	S->Unequip(Cid, EDCGearSlot::Tool);
	TestEqual(TEXT("unequip to stash"), S->GetStash().Num(), 1);
	TestTrue(TEXT("equip from stash"), S->EquipFromStash(TEXT("pepper_spray"), Cid));
	TestEqual(TEXT("stash empty"), S->GetStash().Num(), 0);

	// Clinic patch-ups.
	A::SetRoom(*S, EDCRoomType::Clinic, 2);
	FDCCriminal& C = A::Roster(*S)[0];
	C.Status = EDCCriminalStatus::Injured;
	C.InjuredUntil = S->GetRunTime() + 100.0;
	TestEqual(TEXT("patch cost"), S->GetPatchUpCost(Cid), 800);
	TestTrue(TEXT("patch"), S->PatchUp(Cid));
	TestEqual(TEXT("healed"), S->FindCriminal(Cid)->Status, EDCCriminalStatus::Available);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimInjuryArrest, "DirectorsCut.Sim.InjuryArrest", DCSimFlags)
bool FDCSimInjuryArrest::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(13);
	A::ClearRun(*S);
	// Injury healing.
	const int32 Id = A::AddCriminal(*S, EDCCriminalType::Muscle, 1, { TEXT("brawler") }).Id;
	A::Roster(*S)[0].Status = EDCCriminalStatus::Injured;
	A::Roster(*S)[0].InjuredUntil = S->GetRunTime() + 50.0;
	int32 Healed = 0;
	S->OnCriminalHealed.AddLambda([&](int32) { ++Healed; });
	S->AdvanceTime(49.0);
	TestEqual(TEXT("still injured"), S->FindCriminal(Id)->Status, EDCCriminalStatus::Injured);
	S->AdvanceTime(2.0);
	TestEqual(TEXT("healed"), S->FindCriminal(Id)->Status, EDCCriminalStatus::Available);
	TestEqual(TEXT("healed event"), Healed, 1);

	// Arrests: force Busted (band 1) with arrest risk 1.
	A::Data(*S).Balance.BustedBand = 1.f;
	A::Data(*S).Balance.InjuryBustedChance = 1.f;
	A::AddPerk(*S, TEXT("clean_hands"));
	const int32 Id2 = A::AddCriminal(*S, EDCCriminalType::Ghost, 1, { TEXT("shadow") }).Id;
	FDCMission& M = A::AddMission(*S, 1, 2, { EDCThreat::Guards }, 60.f);
	M.ArrestRisk = 1.f;
	A::ForceRun(*S, M, { Id, Id2 }, 0.f);
	S->AdvanceTime(61.0);
	const FDCMissionReport& R = S->GetReports()[0];
	TestEqual(TEXT("busted"), R.Outcome, EDCMissionOutcome::Busted);
	TestEqual(TEXT("first arrest prevented"), R.Members[0].bArrestPrevented, true);
	TestEqual(TEXT("second arrested"), R.Members[1].bArrested, true);
	TestEqual(TEXT("fallen"), S->GetFallen().Num(), 1);
	TestEqual(TEXT("roster"), S->GetRoster().Num(), 1);
	TestTrue(TEXT("injured"), R.Members[0].bInjured && S->FindCriminal(Id)->Status == EDCCriminalStatus::Injured);
	TestEqual(TEXT("injury loyalty"), R.Members[0].LoyaltyDelta, -3);
	TestEqual(TEXT("no bail without lawyer"), S->GetBailCost(Id2), -1);
	A::SetRoom(*S, EDCRoomType::LawyerOffice, 1);
	TestEqual(TEXT("bail cost"), S->GetBailCost(Id2), 1500);
	A::Cash(*S) = 5000;
	TestTrue(TEXT("bail"), S->BailOut(Id2));
	TestEqual(TEXT("back"), S->GetRoster().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimPerksXP, "DirectorsCut.Sim.PerksAndXP", DCSimFlags)
bool FDCSimPerksXP::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(17);
	A::ClearRun(*S);
	const int32 Id = A::AddCriminal(*S, EDCCriminalType::Muscle, 1, { TEXT("brawler") }).Id;
	int32 Offers = 0, LevelUps = 0;
	S->OnPerkOffered.AddLambda([&](const TArray<FDCPerk>& O) { ++Offers; });
	S->OnCriminalLevelUp.AddLambda([&](int32, int32) { ++LevelUps; });
	for (int32 i = 0; i < 7; ++i)
	{
		FDCMission& M = A::AddMission(*S, 1, 1, { EDCThreat::Guards }, 60.f, 0, 1000);
		M.XPReward = 60;
		A::ForceRun(*S, M, { Id }, 2.f);
		S->AdvanceTime(60.0);
		if (i == 1) { TestFalse(TEXT("no offer after 2"), S->HasPendingPerkOffer()); }
		if (i == 2) { TestTrue(TEXT("offer after 3"), S->HasPendingPerkOffer()); TestEqual(TEXT("offer size"), S->GetPerkOffer().Num(), 3); }
	}
	TestEqual(TEXT("successes"), S->GetMissionsSucceeded(), 7);
	TestEqual(TEXT("2 offers (one queued)"), Offers, 1);
	TestTrue(TEXT("pick"), S->PickPerk(S->GetPerkOffer()[0].Id));
	TestEqual(TEXT("queued offer shows"), Offers, 2);
	TestTrue(TEXT("pending again"), S->HasPendingPerkOffer());
	TestTrue(TEXT("pick 2"), S->PickPerk(S->GetPerkOffer()[0].Id));
	TestEqual(TEXT("perks taken"), S->GetPerksTaken().Num(), 2);
	TestFalse(TEXT("no more"), S->HasPendingPerkOffer());
	// 7 x 60 XP = 420: L1 needs 100, L2 needs round(100*2^1.4)=264 -> level 3 with 56 xp.
	const FDCCriminal* C = S->FindCriminal(Id);
	TestEqual(TEXT("level"), C->Level, 3);
	TestEqual(TEXT("xp"), C->XP, 420 - 100 - 264);
	TestEqual(TEXT("level ups"), LevelUps, 2);
	TestEqual(TEXT("loyalty from fair cuts"), C->Loyalty, FMath::Min(100, 50 + 7 * 5));
	TestEqual(TEXT("tier unlocked by successes"), S->GetUnlockedTier(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimSaveLoad, "DirectorsCut.Sim.SaveLoad", DCSimFlags)
bool FDCSimSaveLoad::RunTest(const FString&)
{
	const TCHAR* Slot = TEXT("DCRun_TestSave");
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim(Slot, true);
	S->StartNewRun(1234);
	TestTrue(TEXT("saved on new run"), S->HasSave());
	// Start a mission, run a bit, save.
	DCSimTest::BotStep(*S);
	TestTrue(TEXT("something running"), S->GetActiveMissionCount() > 0);
	S->AdvanceTime(25.0);
	S->SaveRun();
	const FString Before = DCSimTest::DescribeState(*S);

	TStrongObjectPtr<UDCRunSubsystem> L = DCSimTest::NewSim(Slot, true);
	TestTrue(TEXT("has save"), L->HasSave());
	TestTrue(TEXT("continue"), L->ContinueRun());
	TestEqual(TEXT("round trip"), DCSimTest::DescribeState(*L), Before);
	TestEqual(TEXT("time restored"), L->GetRunTime(), S->GetRunTime());

	// Both continue identically (RNG state restored) and the mission completes at the same time.
	A::EnableSave(*S, false);
	A::EnableSave(*L, false);
	S->AdvanceTime(400.0);
	L->AdvanceTime(400.0);
	TestTrue(TEXT("mission completed"), L->GetReports().Num() > 0);
	TestEqual(TEXT("same future"), DCSimTest::DescribeState(*L), DCSimTest::DescribeState(*S));

	A::EnableSave(*L, true);
	L->AbandonRun();
	TestFalse(TEXT("abandon deletes"), L->HasSave());
	A::EnableSave(*S, false);
	S->ExitToTitle();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimDeterminism, "DirectorsCut.Sim.Determinism", DCSimFlags)
bool FDCSimDeterminism::RunTest(const FString&)
{
	auto Run = [](int32 Seed)
	{
		TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
		DCSimTest::FEndGuard G(S.Get());
		S->StartNewRun(Seed);
		for (int32 i = 0; i < 240 && S->GetPhase() == EDCGamePhase::Playing; ++i) { DCSimTest::BotStep(*S); S->AdvanceTime(5.0); }
		return DCSimTest::DescribeState(*S);
	};
	const FString A1 = Run(777), A2 = Run(777), B1 = Run(778);
	TestEqual(TEXT("same seed same run"), A1, A2);
	TestNotEqual(TEXT("different seed differs"), A1, B1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimBigScore, "DirectorsCut.Sim.BigScore", DCSimFlags)
bool FDCSimBigScore::RunTest(const FString&)
{
	TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
	DCSimTest::FEndGuard G(S.Get());
	S->StartNewRun(21);
	A::ClearRun(*S);
	A::Successes(*S) = 15;
	S->AdvanceTime(1.0);
	TestFalse(TEXT("needs ops room"), S->GetMissions().ContainsByPredicate([](const FDCMission& M) { return M.bFinal; }));
	A::SetRoom(*S, EDCRoomType::OpsRoom, 1);
	TestEqual(TEXT("tier 4"), S->GetUnlockedTier(), 4);
	S->AdvanceTime(1.0);
	const FDCMission* F = S->GetMissions().FindByPredicate([](const FDCMission& M) { return M.bFinal; });
	TestNotNull(TEXT("big score posted"), F);
	if (!F) { return false; }
	TestTrue(TEXT("never expires"), F->ExpiresAt < 0.0);
	TestEqual(TEXT("5 crew"), F->CrewSize, 5);
	S->AdvanceTime(2000.0);
	TestEqual(TEXT("only one"), S->GetMissions().FilterByPredicate([](const FDCMission& M) { return M.bFinal; }).Num(), 1);
	A::Cash(*S) = 1000000;
	FDCMission* FM = A::Missions(*S).FindByPredicate([](const FDCMission& M) { return M.bFinal; });
	TArray<int32> Crew;
	for (int32 i = 0; i < 5; ++i) { Crew.Add(A::AddCriminal(*S, (EDCCriminalType)i, 10, {}).Id); }
	bool bWon = false;
	S->OnRunWon.AddLambda([&](const FDCRunSummary& Sum) { bWon = Sum.bWon; });
	A::ForceRun(*S, *FM, Crew, 2.f);
	S->AdvanceTime(FM->Duration + 1.0);
	TestTrue(TEXT("won"), bWon);
	TestEqual(TEXT("phase won"), S->GetPhase(), EDCGamePhase::Won);
	return true;
}

// ─── bot soak ───────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSimSoak, "DirectorsCut.Sim.BotSoak", DCSimFlags)
bool FDCSimSoak::RunTest(const FString&)
{
	for (int32 Seed : { 101, 202, 303, 404 })
	{
		TStrongObjectPtr<UDCRunSubsystem> S = DCSimTest::NewSim();
		DCSimTest::FEndGuard G(S.Get());
		S->StartNewRun(Seed);
		int64 Expected = S->GetCash();
		int32 SeenReports = 0;
		S->OnPayday.AddLambda([&](int32, int32 Amt) { Expected -= Amt; });
		S->OnMissionCompleted.AddLambda([&](const FDCMissionReport& R) { Expected += R.DirectorTake; ++SeenReports; });
		S->OnMissionStarted.AddLambda([&](const FDCMission& M) { Expected -= M.Cost; });
		S->OnCriminalHired.AddLambda([&](const FDCCriminal&) { Expected = -1; }); // hire cost: resync below
		S->OnRoomChanged.AddLambda([&](const FDCRoom&) {});
		int32 Started = 0;
		bool bInvariantOk = true;
		for (double T = 0.0; T < 90.0 * 60.0 && S->GetPhase() == EDCGamePhase::Playing; T += 5.0)
		{
			const int32 CashBefore = S->GetCash();
			const int32 RoomsBefore = S->GetRooms().FilterByPredicate([](const FDCRoom& R) { return R.State == EDCRoomState::Building; }).Num();
			Expected = CashBefore;
			Started += DCSimTest::BotStep(*S);
			Expected = S->GetCash(); // spending in BotStep is checked by the individual tests; resync
			S->AdvanceTime(5.0);
			if (S->GetPhase() == EDCGamePhase::Playing && S->GetCash() != Expected)
			{
				AddError(FString::Printf(TEXT("seed %d t=%.0f cash %d != expected %lld"), Seed, S->GetRunTime(), S->GetCash(), Expected));
				bInvariantOk = false;
				break;
			}
			// invariants
			TMap<int32, int32> OnMission;
			for (const FDCMission& M : S->GetMissions())
			{
				if (!M.bInProgress) { continue; }
				if (S->GetMissionTimeRemaining(M.Id) < 0.0 || M.EndsAt < M.StartedAt || M.EndsAt < S->GetRunTime()) { AddError(TEXT("bad mission timer")); bInvariantOk = false; }
				for (int32 Id : M.CrewIds) { OnMission.FindOrAdd(Id)++; }
			}
			for (const TPair<int32, int32>& P : OnMission) { if (P.Value > 1) { AddError(TEXT("crew on two missions")); bInvariantOk = false; } }
			for (const FDCCriminal& C : S->GetRoster())
			{
				const bool bOn = C.Status == EDCCriminalStatus::OnMission;
				if (bOn != OnMission.Contains(C.Id)) { AddError(TEXT("status/mission mismatch")); bInvariantOk = false; }
				if (C.Status == EDCCriminalStatus::Injured && C.InjuredUntil < S->GetRunTime()) { AddError(TEXT("stale injury")); bInvariantOk = false; }
			}
			if (S->GetNextMissionIn() < 0.0 || S->GetRecruitRefreshIn() < 0.0 || S->GetSecondsUntilPayday() < 0.0) { AddError(TEXT("negative timer")); bInvariantOk = false; }
			if (!bInvariantOk) { break; }
			if (FMath::Fmod(S->GetRunTime(), 600.0) < 5.0 - 1e-6)
			{
				int32 Built = 0;
				for (const FDCRoom& R : S->GetRooms()) { if (R.Type != EDCRoomType::Office) { Built += R.Level; } }
				UE_LOG(LogDC, Display, TEXT("[Soak] seed %d t=%4.0fm day %d cash %6d succ %3d/%3d tier %d roster %d rooms %d perks %d fallen %d"),
					Seed, S->GetRunTime() / 60.0, S->GetDay(), S->GetCash(), S->GetMissionsSucceeded(), S->GetRunSummary().MissionsAttempted,
					S->GetUnlockedTier(), S->GetRoster().Num(), Built, S->GetPerksTaken().Num(), S->GetFallen().Num());
			}
		}
		const FDCRunSummary Sum = S->GetRunSummary();
		UE_LOG(LogDC, Display, TEXT("[Soak] seed %d END phase %d (%s) day %d cash %d succ %d/%d take %d rooms %d tier %d lost %d"),
			Seed, (int32)S->GetPhase(), *Sum.EndReason, Sum.DaysSurvived, S->GetCash(), Sum.MissionsSucceeded, Sum.MissionsAttempted, Sum.TotalTake, Sum.RoomsBuilt, S->GetUnlockedTier(), Sum.Lost.Num());
		TestTrue(TEXT("bot started missions"), Started > 0);
	}
	return true;
}

#endif
